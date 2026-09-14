# Pack files in STAGE_DIR at the zip root (no nested folder, no zip-in-zip).
#   cmake -DSTAGE_DIR=... -DZIP=... -P cmake/package_zip.cmake

if (NOT STAGE_DIR OR NOT ZIP)
  message (FATAL_ERROR "package_zip.cmake requires STAGE_DIR and ZIP")
endif ()
if (NOT IS_DIRECTORY "${STAGE_DIR}")
  message (FATAL_ERROR "STAGE_DIR is not a directory: ${STAGE_DIR}")
endif ()

file (GLOB _names RELATIVE "${STAGE_DIR}" "${STAGE_DIR}/*")
if (NOT _names)
  message (FATAL_ERROR "STAGE_DIR is empty: ${STAGE_DIR}")
endif ()

# Never nest a previous archive inside the new one.
list (FILTER _names EXCLUDE REGEX "\\.zip$")

execute_process (
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP}" --format=zip -- ${_names}
  WORKING_DIRECTORY "${STAGE_DIR}"
  RESULT_VARIABLE _rc
)
if (_rc)
  message (FATAL_ERROR "Failed to write ${ZIP}")
endif ()
