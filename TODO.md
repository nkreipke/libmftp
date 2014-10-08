# TODO List
* MDTM implementation is missing (```ftp_modification_date(...)```)
* TLS-enabled connection sometimes loses a few bytes at the end when writing to a file. This has to be looked into.
* ```ftptls.c``` is still a mess and needs some clean-up.
* Similar to ```_current_features```, multiple simultaneous connections should only have one single error variable, so having a separate function to retrieve file related errors is not needed anymore.
* There may be a race condition between ```ftp_i_wait_for_triggers``` calls and processing of the signal, as a new unrelated signal could possibly override _last_signal before it has been processed. It would be better to pass a second parameter to ```ftp_i_wait_for_triggers``` that specifies a destination for this particular waiting process.