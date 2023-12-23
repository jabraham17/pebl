set(DRIVER_MAIN "driver.py")
set(DRIVER_FILES "driver/utils.py" "driver/paths.py")

install(FILES ${DRIVER_FILES} DESTINATION bin/driver)
install(
  PROGRAMS ${DRIVER_MAIN}
  DESTINATION bin
  RENAME pebl)
