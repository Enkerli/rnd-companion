# Writes BuildStamp.h to OUTPUT every time it runs.
# Invoked as a build-time custom command so the stamp is always current.
#   cmake -DOUTPUT=<path> -P GenBuildStamp.cmake
#
# The suite learned the hard way that a build-id chip is the first thing to
# check when the UI and the engine disagree about what is running.
string(TIMESTAMP STAMP "%Y-%m-%d %H:%M")
file(WRITE "${OUTPUT}" "#pragma once\n#define RND_BUILD_STAMP \"${STAMP}\"\n")
