add_executable (concept_client src/conceptclient.cpp)

target_link_libraries (concept_client
	example_distributed_client
	example_distributed_core
	foundation_ipc
	foundation_aio
	foundation_core
	algorithm_serialization
	utility
	system
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
)

add_executable (concept_server src/conceptserver.cpp)

target_link_libraries (concept_server
	example_distributed_server
	example_distributed_core
	foundation_ipc
	foundation_aio
	foundation_core
	algorithm_serialization
	utility
	system
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
)





