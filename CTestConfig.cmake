
message("Testing enabled: testname = ${UTEST_NAME} testsite = ${UTEST_SITE}")
if(UTEST_NAME AND UTEST_SITE)
	message("CDash dropping acctivated")
	set(CTEST_PROJECT_NAME "${UTEST_NAME}")
	#set(CTEST_NIGHTLY_START_TIME "01:00:00 UTC")
	
	set(CTEST_DROP_METHOD "http")
	
	set(CTEST_DROP_SITE "${UTEST_SITE}")
	set(CTEST_DROP_LOCATION "/CDash/submit.php?project=${UTEST_NAME}")
	set(CTEST_DROP_SITE_CDASH TRUE)
else(UTEST_NAME AND UTEST_SITE)
	message("CDash dropping not acctivated")
endif(UTEST_NAME AND UTEST_SITE)