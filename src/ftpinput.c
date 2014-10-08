/*   libmftp
 *
 *   Copyright (c) 2014 nkreipke
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */


#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include "ftpinternal.h"
#include "ftpcommands.h"

#define SIGN_TERMINATE (-1)
#define SIGN_NOTHING 0

void *   ftp_i_input_thread(void *);
ftp_bool ftp_i_is_trigger(ftp_connection *, int);
void     ftp_i_reset_triggers(ftp_connection *);
#define  ftp_i_has_triggers(c) (c->_input_trigger_signals[0] != SIGN_NOTHING)
ftp_bool ftp_i_reached_timeout(ftp_connection *);
ftp_bool ftp_i_process_input(ftp_connection *, ftp_i_managed_buffer *);

/*
 * This function runs in a background thread and receives messages from the server.
 * When an operation waits for a specific server answer, it sets a "trigger signal", which
 * this function will recognize and terminate. After signal processing is done, a new
 * input thread is established.
 */
void* ftp_i_input_thread(void *connection)
{
	ftp_connection *c = (ftp_connection *)connection;

	ftp_i_managed_buffer *message = ftp_i_managed_buffer_new();

	pthread_mutex_t *locked_mutex = NULL;

	while (!c->_release_input_thread) {
		char current;
		if (ftp_i_read(c, 0, &current, 1) == 1) {
			if (current == CHAR_LF)
				// Ignoring newline characters.
				continue;
			if (current == CHAR_CR) {
				// Buffer contains a complete message.
				// We have to consume the following Line Feed, otherwise this could irritate an SSL
				// handshake.
				if (ftp_i_read(c, 0, &current, 1) != 1 || current != CHAR_LF) {
					// Unexpected input.
					FTP_ERR("Input thread received invalid char after Carriage Return.\n");
					ftp_i_connection_set_error(c, FTP_EUNEXPECTED);
					break;
				}

				// Now lock ourselves down while we process this message.
				if (pthread_mutex_lock(&c->_input_thread_processing_signal) != 0) {
					FTP_ERR("Could not lock signal processing mutex.\n");
					ftp_i_connection_set_error(c, FTP_ETHREAD);
					break;
				}
				locked_mutex = &c->_input_thread_processing_signal;

				if (ftp_i_process_input(c, message)) {
					// Processed message requires this input thread to terminate in order to
					// notify the waiting main thread.
					break;
				}

				// Reset message buffer.
				ftp_i_managed_buffer_free(message);
				message = ftp_i_managed_buffer_new();

				locked_mutex = NULL;
				pthread_mutex_unlock(&c->_input_thread_processing_signal);

			} else {
				if (ftp_i_managed_buffer_append(message, (void *)&current, 1) != FTP_OK) {
					FTP_ERR("Allocation error.\n");
					ftp_i_connection_set_error(c, FTP_ECOULDNOTALLOCATE);
					break;
				}
			}
		} else {
			if (ftp_i_is_timed_out(errno)) {
				// We reached a timeout. The timeout is set to a short time span,
				// so we are able to react appropriately.
				if (ftp_i_connection_is_waiting(c) && ftp_i_reached_timeout(c)) {
					// The connection currently waits for a server answer and has reached
					// the pre-defined timeout.
					FTP_ERR("Timeout reached.\n");
					ftp_i_connection_set_error(c, FTP_ETIMEOUT);
					break;
				}
			} else if (ftp_i_connection_is_down(c) || c->_termination_signal) {
				// Connection ended normally.
				break;
			} else {
				// Another socket error, we are probably not able to continue from here.
				FTP_ERR("Socket Error.\n");
				ftp_i_connection_set_error(c, FTP_ESOCKET);
				break;
			}
		}
	}

	ftp_i_managed_buffer_free(message);
	c->_input_thread = 0;

	if (locked_mutex)
		pthread_mutex_unlock(locked_mutex);

	return NULL;
}

/*
 * Processes raw input bytes from the server. An input message usually starts with a
 * three-digit code and may contain further information appended to it.
 * This function returns ftp_btrue if the processed signal is a trigger or an error signal.
 */
ftp_bool ftp_i_process_input(ftp_connection *c, ftp_i_managed_buffer *buf)
{
	int signal;
	ftp_bool is_error;

	if (ftp_i_managed_buffer_length(buf) < 3)
		return ftp_bfalse;

	if ((signal = ftp_i_input_sign(ftp_i_managed_buffer_cbuf(buf))) == FTP_INTERNAL_SIGNAL_ERROR)
		return ftp_bfalse;

#ifdef FTP_SERVER_VERBOSE
	printf("# [server->client] ");
	if (c->_temporary)
		printf("(TMP) ");
	ftp_i_managed_buffer_print(buf, ftp_btrue);
#endif

	c->last_signal = signal;
	is_error = ftp_i_signal_is_error(signal);
	if (is_error)
		c->_internal_error_signal = ftp_btrue;

	if (c->_last_answer_lock_signal != SIGN_NOTHING && c->_last_answer_lock_signal == signal) {
		// Store the string attached to the signal number.
		if (c->_last_answer_buffer) {
			FTP_WARN("BUG: _last_answer_buffer is not empty.\n");
			ftp_i_managed_buffer_free(c->_last_answer_buffer);
		}
		ftp_i_managed_buffer *last_answer = ftp_i_managed_buffer_new();
		if (ftp_i_managed_buffer_memcpy(last_answer, buf, 4, ftp_i_managed_buffer_length(buf) - 4) != FTP_OK) {
			FTP_ERR("Allocation error.\n");
			ftp_i_managed_buffer_free(last_answer);
			return ftp_bfalse;
		}
		c->_last_answer_buffer = (void *)last_answer;
	}

	if (ftp_i_has_triggers(c))
		// This connection waits for something. We will return true if a trigger signal was
		// reached and also if the signal is an error.
		if (is_error || ftp_i_is_trigger(c, signal))
			return ftp_btrue;

	return ftp_bfalse;
}

/*
 * Determines whether the signal wait timeout is reached.
 */
ftp_bool ftp_i_reached_timeout(ftp_connection *c)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	long sec = ftp_i_seconds_between(c->_wait_start, t);
	return (sec > c->timeout);
}

/*
 * Establishes a new input thread.
 */
int ftp_i_establish_input_thread(ftp_connection *c)
{
	if (c->_disable_input_thread)
		FTP_WARN("BUG: trying to establish an input thread although _disable_input_thread is true.\n");
	if (c->_input_thread != 0)
		FTP_WARN("BUG: trying to establish an input thread while an instance already esists.\n");
	pthread_t t;
	int r = pthread_create(&t, NULL, ftp_i_input_thread, c);
	c->_input_thread = t;
	return r;
}

/*
 * Waits for the input thread to receive a trigger or an error signal.
 * This will afterwards reset all triggers.
 */
ftp_status ftp_i_wait_for_triggers(ftp_connection *c)
{
	if (c->status != FTP_UP && c->status != FTP_CONNECTING) {
		ftp_i_connection_set_error(c, FTP_ENOTREADY);
		return FTP_ERROR;
	}

	// The input thread will automatically terminate when an input trigger
	// is reached.
	c->status = FTP_WAITING;
	ftp_i_connection_set_error(c, 0);
	gettimeofday(&c->_wait_start, NULL);

	if (c->_input_thread != 0 && pthread_join(c->_input_thread, NULL) != 0) {
		FTP_ERR("Could not join input thread.\n");
		ftp_i_connection_set_error(c, FTP_ETHREAD);
		return FTP_ERROR;
	}

	ftp_bool result = FTP_OK;
	if (c->error != 0)
		// An error occurred while waiting for the trigger (timeout, socket error, ...)
		result = FTP_ERROR;

	ftp_i_reset_triggers(c);
	c->_last_answer_lock_signal = SIGN_NOTHING;

	if (!c->_disable_input_thread && ftp_i_establish_input_thread(c) != 0) {
		FTP_ERR("Could not establish input thread.\n");
		ftp_i_connection_set_error(c, FTP_ETHREAD);
		result = FTP_ERROR;
	}

	c->status = FTP_UP;
	return result;
}

/*
 * Starts waiting asynchronously for a trigger or an error signaĺ.
 */
ftp_status ftp_i_start_waiting_async_for_triggers(ftp_connection *c)
{
	if (c->status != FTP_UP) {
		ftp_i_connection_set_error(c, FTP_ENOTREADY);
		return FTP_ERROR;
	}

	c->status = FTP_ASYNCWAITING;
	ftp_i_connection_set_error(c, 0);

	return FTP_OK;
}

/*
 * When asynchronously waiting for triggers, this function returns whether a trigger
 * or error signal has been received.
 */
ftp_bool ftp_i_reached_trigger(ftp_connection *c)
{
	if (c->status != FTP_ASYNCWAITING)
		return ftp_bfalse;

	// Waiting asynchronously uses the same technique as the synchronous wait:
	// The input thread automatically terminates when a trigger is reached. While
	// using pthread_join in the synchronous version, the async version just checks
	// whether the input thread is still running.
	return !(c->_input_thread);
}

/*
 * Ends waiting asynchronously for a trigger or an error signal. If abort is set to
 * ftp_bfalse and a trigger was not yet reached, this function will behave like
 * ftp_i_wait_for_triggers.
 */
ftp_status ftp_i_end_waiting_async_for_triggers(ftp_connection *c, ftp_bool abort)
{
	if (c->status != FTP_ASYNCWAITING)
		return FTP_OK;

	if (!ftp_i_reached_trigger(c) && !abort) {
		// Wait synchronously for trigger. We have to set the connection status to
		// FTP_UP, otherwise ftp_i_wait_for_triggers fails with FTP_ENOTREADY.

		c->status = FTP_UP;
		return ftp_i_wait_for_triggers(c);
	}

	// Do not wait for any trigger.

	// When using the async functions, we have to be careful to avoid race conditions.
	// Locking _input_thread_processing_signal here makes sure our input thread is
	// not currently processing a server message.
	// After the mutex is locked, the input thread can be in two states:
	//  * No trigger was reached -> input thread is still running
	//  * Trigger was reached -> input thread is terminated, _input_thread is 0
	// While the mutex is locked, the input thread is unable to terminate itself.

	if (pthread_mutex_lock(&c->_input_thread_processing_signal) != 0) {
		ftp_i_connection_set_error(c, FTP_ETHREAD);
		return FTP_ERROR;
	}

	ftp_status result = FTP_OK;

	ftp_i_reset_triggers(c);
	c->_last_answer_lock_signal = SIGN_NOTHING;

	if (!c->_input_thread &&
			!c->_disable_input_thread &&
			ftp_i_establish_input_thread(c) != 0) {

		FTP_ERR("Could not establish input thread.\n");
		ftp_i_connection_set_error(c, FTP_ETHREAD);
		result = FTP_ERROR;

	} else {
		c->status = FTP_UP;
	}

	pthread_mutex_unlock(&c->_input_thread_processing_signal);

	return result;
}

/*
 * Terminates an input thread.
 */
int ftp_i_release_input_thread(ftp_connection *c) {
	c->_release_input_thread = ftp_btrue;
	if (c->_input_thread != 0)
		pthread_join(c->_input_thread, NULL);
	c->_release_input_thread = ftp_bfalse;
	return 0;
}

/*
 * Sets a trigger signal.
 */
void ftp_i_set_input_trigger(ftp_connection *c, int sig)
{
	for (int i = 0; i < FTP_TRIGGER_MAX; i++) {
		if (c->_input_trigger_signals[i] == SIGN_NOTHING) {
			c->_input_trigger_signals[i] = sig;
			return;
		}
	}
	FTP_WARN("BUG: Too many trigger signals registered.\n");
}

/*
 * Determines whether a signal is a trigger signal.
 */
ftp_bool ftp_i_is_trigger(ftp_connection *c, int sig)
{
	for (int i = 0; i < FTP_TRIGGER_MAX; i++) {
		if (c->_input_trigger_signals[i] == SIGN_NOTHING)
			return ftp_bfalse;
		if (c->_input_trigger_signals[i] == sig)
			return ftp_btrue;
	}
	return ftp_bfalse;
}

/*
 * Resets all triggers.
 */
void ftp_i_reset_triggers(ftp_connection *c)
{
	for (int i = 0; i < FTP_TRIGGER_MAX; i++)
		c->_input_trigger_signals[i] = SIGN_NOTHING;
}
