    set(BinFiles
            "hakc-static-analysis"
            "hakc-policy-process"
    )

    file(GLOB HAKC_MODS hakc/*.py)

    foreach (BinFile ${BinFiles})
        install(PROGRAMS ${BinFile} DESTINATION ${CMAKE_INSTALL_BINDIR}/hakc/ COMPONENT ${BinFile})
        add_custom_target(${BinFile} ALL DEPENDS ${BinFile})
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/hakc/${BinFile}
                COMMAND ${CMAKE_COMMAND} -E copy
                ${BinFile}
                ${CMAKE_BINARY_DIR}/bin/hakc/${BinFile}
                DEPENDS ${BinFile})
        if (NOT LLVM_ENABLE_IDE)
            add_llvm_install_targets("install-${BinFile}"
                    DEPENDS ${BinFile}
                    COMPONENT ${BinFile})
        endif ()
    endforeach ()

    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/hakc/requirements.txt
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
            ${CMAKE_BINARY_DIR}/bin/hakc
            DEPENDS requirements.txt)
    install(PROGRAMS requirements.txt
            DESTINATION ${CMAKE_INSTALL_BINDIR}/hakc
            COMPONENT hakc-policy-process)

    foreach (HAKCMod ${HAKC_MODS})
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bin/hakc/hakc/${HAKCMod}
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/hakc/${HAKCMod}
                ${CMAKE_BINARY_DIR}/bin/hakc/hakc/${HAKCMod}
                DEPENDS ${HAKCMod})
        install(PROGRAMS ${HAKCMod}
                DESTINATION "${CMAKE_INSTALL_BINDIR}/hakc/hakc"
                COMPONENT hakc-policy-process)
    endforeach ()
