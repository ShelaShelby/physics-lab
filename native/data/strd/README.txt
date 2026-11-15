NIST Statistical Reference Datasets (StRD), nonlinear regression: the test data of the Data panel's fitting (tests/test_analysis.cpp, 0.51)
=============================================================================================================================================

What: the 26 nonlinear-regression problems of NIST's StRD with one predictor (all of them but Nelson, which has two), each with NIST's two starting
points and its CERTIFIED parameters, standard deviations, residual sum of squares and residual standard deviation. The tests read these numbers from
the files themselves, so nothing here was typed by hand.
  NIST Information Technology Laboratory, Statistical Engineering Division: Statistical Reference Datasets,
  https://www.itl.nist.gov/div898/strd/nls/nls_main.shtml (each file's own header names the original reference of its data).

How it was read (2026-11-13): www.itl.nist.gov refused the cloud session (the proxy answered 403 to the connection), so the files were fetched with
curl from the copy kept in the lmfit-py repository, https://raw.githubusercontent.com/lmfit/lmfit-py/master/NIST_STRD/<Name>.dat, and saved exactly as
received. Each begins "NIST/ITL StRD" and is NIST's own file format; they were not compared byte for byte with NIST's site (it could not be reached).
The owner may want to compare one with the NIST page once.

A known misprint kept as it is: Rat43.dat prints 9 degrees of freedom for 15 observations and 4 parameters; its certified residual standard deviation is
sqrt(RSS / 11), so the 9 is wrong (the test checks the RSD against RSS / (rows - parameters) for every file).
