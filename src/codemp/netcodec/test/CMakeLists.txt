# netcodec's tests. Some read traces made locally from recordings (not committed): the rANS layer against the Python
# reference coder (export_rans_trace.py), the round trip of the format on the corpus (export_demo_traces.py), the
# client packets (netcodec_bench_client --trace); the others need none.

# The rANS layer against the Python reference coder
netcodec_add_program(netcodec_test_rans test_rans.cpp)
add_test(NAME netcodec.rans_golden COMMAND netcodec_test_rans "${netcodec_TRACE_DIR}/rans_trace.bin")

# The round trip of the format on the corpus traces: white-box (it reads the schema to make -0.0 values). The hash of
# the coded bytes is the same on every compiler, architecture and build type; it changes with the format, the tables
# or the traces: update it from the test's output (clang Release), then check the other presets.
set(netcodec_ROUNDTRIP_HASH "0x3E60F50570AA2C01")
netcodec_add_program(netcodec_test_roundtrip test_roundtrip.cpp)
target_link_libraries(netcodec_test_roundtrip PRIVATE netcodec_internal)
add_test(NAME netcodec.roundtrip COMMAND netcodec_test_roundtrip "--hash=${netcodec_ROUNDTRIP_HASH}"
	"${netcodec_TRACE_DIR}/captures.bin" "${netcodec_TRACE_DIR}/holdout.bin")

# The engine's interface (engine.h) on the corpus, with engine-like storage: the same coded bytes as the round trip
netcodec_add_program(netcodec_test_engine test_engine.cpp)
target_link_libraries(netcodec_test_engine PRIVATE netcodec_internal)
add_test(NAME netcodec.engine COMMAND netcodec_test_engine "--hash=${netcodec_ROUNDTRIP_HASH}"
	"${netcodec_TRACE_DIR}/captures.bin" "${netcodec_TRACE_DIR}/holdout.bin")
# the two hold the corpus in memory: one at a time when the tests run in parallel
set_tests_properties(netcodec.roundtrip netcodec.engine PROPERTIES RESOURCE_LOCK netcodec_corpus)

# The determinism of the float steps (angle indices) against the Python reference (angle_hash.py)
netcodec_add_program(netcodec_test_determinism test_determinism.cpp)
target_link_libraries(netcodec_test_determinism PRIVATE netcodec_internal)
add_test(NAME netcodec.determinism COMMAND netcodec_test_determinism)

# A synthetic round trip (every operation, vehicles, pilots, extreme values, base ages up to 255): 100 streams of
# 1,000 messages. The hash of the coded bytes changes with the format, the tables or the generator.
set(netcodec_SYNTHETIC_HASH "0x71CFD4AED775CF37")
netcodec_add_program(netcodec_test_synthetic test_synthetic.cpp)
target_link_libraries(netcodec_test_synthetic PRIVATE netcodec_internal)
add_test(NAME netcodec.synthetic COMMAND netcodec_test_synthetic "--hash=${netcodec_SYNTHETIC_HASH}" 100 1000)

# Malformed input: mutated copies of synthetic messages decoded with the client's state (20 streams of 500 messages,
# 20 mutations each); no fault, and decode(encode(decode(x))) == decode(x) for those that decode
netcodec_add_program(netcodec_test_decode_robustness test_decode_robustness.cpp)
target_link_libraries(netcodec_test_decode_robustness PRIVATE netcodec_internal)
add_test(NAME netcodec.decode_robustness COMMAND netcodec_test_decode_robustness 20 500 20)

# The client packets: a synthetic round trip, and the round trip of the client traces, with and without base. Each
# hash changes with the client format or tables.
set(netcodec_CLIENT_SYNTHETIC_HASH "0xCFE7E7FEB2FB9F06")
set(netcodec_CLIENT_ROUNDTRIP_HASH "0xC371DB108DA14F6D")
netcodec_add_program(netcodec_test_client test_client.cpp)
target_link_libraries(netcodec_test_client PRIVATE netcodec_internal)
add_test(NAME netcodec.client_synthetic COMMAND netcodec_test_client "--hash=${netcodec_CLIENT_SYNTHETIC_HASH}"
	synthetic 100000)
add_test(NAME netcodec.client_roundtrip COMMAND netcodec_test_client "--hash=${netcodec_CLIENT_ROUNDTRIP_HASH}"
	"${netcodec_TRACE_DIR}/client_train.bin" "${netcodec_TRACE_DIR}/client_holdout.bin")

# Malformed client packets (the server decodes them from anyone): mutated copies of 20,000 synthetic packets, 20 each,
# decoded with random bases and `latest` values; no fault, decode(encode(decode(x))) == decode(x), and the command
# length limit
netcodec_add_program(netcodec_test_client_robustness test_client_robustness.cpp)
target_link_libraries(netcodec_test_client_robustness PRIVATE netcodec_internal)
add_test(NAME netcodec.client_robustness COMMAND netcodec_test_client_robustness 20000 20)

foreach(trace rans_trace captures holdout client_train client_holdout)
	if(NOT EXISTS "${netcodec_TRACE_DIR}/${trace}.bin")
		message(STATUS "netcodec: ${netcodec_TRACE_DIR}/${trace}.bin not found: the tests that read it will fail")
	endif()
endforeach()

# The determinism of the game's player movement (bg_pmove.cpp) as the game module and as cgame
add_subdirectory(pmove)
