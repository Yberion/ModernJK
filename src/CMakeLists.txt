# The four trees: the code both engines share (shared), the multiplayer (codemp, with netcodec), Jedi Academy's single
# player (code, whose engine Jedi Outcast's also builds from) and Jedi Outcast's game module (codeJK2).
add_subdirectory(shared)
add_subdirectory(codemp)
if(modernjk_BUILD_SP OR modernjk_BUILD_JK2)
	add_subdirectory(code)
endif()
if(modernjk_BUILD_JK2)
	add_subdirectory(codeJK2/game)
endif()
