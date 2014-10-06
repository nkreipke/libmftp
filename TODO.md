# TODO List
* We have to wait for *226 Transfer complete* after the server closes the data connection. Currently it interprets the closing of the connection itself as the success signal and possibly fails to get an error message regarding transfer.
* MDTM implementation is missing (```ftp_modification_date(...)```)
* TLS-enabled connection sometimes loses a few bytes at the end when writing to a file. This has to be looked into.
* ```ftptls.c``` is still a mess and needs some clean-up.
* Similar to ```_current_features```, multiple simultaneous connections should only have one single error variable, so having a separate function to retrieve file related errors is not needed anymore.
