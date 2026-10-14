# Usage: cmake -DINPUT=file -DOUTPUT=file.cpp -DSYMBOL=name -DNAMESPACE=ns -P EmbedText.cmake
# Writes a C++ source file defining `const char* const SYMBOL` with the
# contents of INPUT as a raw string literal.
file(READ "${INPUT}" CONTENT)
string(FIND "${CONTENT}" ")EMBED\"" BAD)
if(NOT BAD EQUAL -1)
  message(FATAL_ERROR "${INPUT} contains the raw-string delimiter")
endif()
file(WRITE "${OUTPUT}.tmp"
  "// Generated from ${INPUT} - do not edit.\n"
  "namespace ${NAMESPACE} {\n"
  "extern const char* const ${SYMBOL};\n"
  "const char* const ${SYMBOL} = R\"EMBED(${CONTENT})EMBED\";\n"
  "}\n")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
