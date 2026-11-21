# The format's tools: netcodec_train trains the tables on the corpus traces (export_demo_traces.py) and writes
# src/generated/jka_tables.inc; netcodec_train_client trains the client packets' tables on the client traces
# (netcodec_bench_client --trace) and writes src/generated/jka_client_tables.inc.
foreach(tool train train_client)
	netcodec_add_program(netcodec_${tool} ${tool}.cpp)
	target_link_libraries(netcodec_${tool} PRIVATE netcodec_traces netcodec_internal)
endforeach()
