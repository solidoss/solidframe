add_executable (example_multi_launcher src/multilauncher.cpp)

target_link_libraries (example_multi_launcher
	system
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
)

add_executable (example_consensus_client src/consensusclient.cpp)

target_link_libraries (example_consensus_client
	example_distributed_consensus_client
	example_distributed_consensus_core
	distributed_consensus_core
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

add_executable (example_consensus_server src/consensusserver.cpp)

target_link_libraries (example_consensus_server
	example_distributed_consensus_server
	example_distributed_consensus_core
	distributed_consensus_server
	distributed_consensus_core
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





