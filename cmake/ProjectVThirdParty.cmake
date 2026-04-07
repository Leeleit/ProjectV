function(_projectv_collect_buildsystem_targets out_var directory_path)
    get_property(local_targets DIRECTORY "${directory_path}" PROPERTY BUILDSYSTEM_TARGETS)
    set(all_targets ${local_targets})

    get_property(child_directories DIRECTORY "${directory_path}" PROPERTY SUBDIRECTORIES)
    foreach (child_directory IN LISTS child_directories)
        _projectv_collect_buildsystem_targets(child_targets "${child_directory}")
        list(APPEND all_targets ${child_targets})
    endforeach ()

    list(REMOVE_DUPLICATES all_targets)
    set(${out_var} "${all_targets}" PARENT_SCOPE)
endfunction()

function(projectv_suppress_external_warnings directory_path)
    _projectv_collect_buildsystem_targets(directory_targets "${directory_path}")
    foreach (target_name IN LISTS directory_targets)
        if (NOT TARGET "${target_name}")
            continue()
        endif ()

        get_target_property(target_type "${target_name}" TYPE)
        if (NOT target_type OR target_type STREQUAL "INTERFACE_LIBRARY" OR target_type STREQUAL "UTILITY")
            continue()
        endif ()

        if (MSVC)
            target_compile_options("${target_name}" PRIVATE /W0)
            if (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(
                        "${target_name}"
                        PRIVATE
                        /clang:-Wno-everything
                        /clang:-Wno-unused-command-line-argument
                )
            endif ()
            target_compile_definitions(
                    "${target_name}"
                    PRIVATE
                    _CRT_SECURE_NO_WARNINGS
                    _CRT_NONSTDC_NO_WARNINGS
            )
        else ()
            target_compile_options("${target_name}" PRIVATE -w)
        endif ()
    endforeach ()
endfunction()

function(projectv_cleanup_ninja_subbuild_state build_root)
    file(
            GLOB_RECURSE
            projectv_ninja_temp_logs
            LIST_DIRECTORIES FALSE
            "${build_root}/_deps/*-subbuild/.ninja_deps"
            "${build_root}/_deps/*-subbuild/.ninja_lock"
            "${build_root}/_deps/*-subbuild/.ninja_log"
            "${build_root}/_deps/*-subbuild/.ninja_*.recompact"
            "${build_root}/_deps/*-subbuild/.ninja_*.restat"
    )

    if (projectv_ninja_temp_logs)
        file(REMOVE ${projectv_ninja_temp_logs})
    endif ()
endfunction()

function(projectv_add_tracy)
    if (PROJECTV_BUILD_TRACY_PROFILER AND NOT PROJECTV_ENABLE_TRACY)
        message(FATAL_ERROR "PROJECTV_BUILD_TRACY_PROFILER requires PROJECTV_ENABLE_TRACY=ON")
    endif ()

    if (NOT PROJECTV_ENABLE_TRACY)
        return()
    endif ()

    set(TRACY_ENABLE ON CACHE BOOL "Enable profiling" FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/tracy" "${CMAKE_BINARY_DIR}/external/tracy" EXCLUDE_FROM_ALL)
    projectv_suppress_external_warnings("${PROJECT_SOURCE_DIR}/external/tracy")

    if (PROJECTV_BUILD_TRACY_PROFILER)
        projectv_cleanup_ninja_subbuild_state("${CMAKE_BINARY_DIR}")
        set(projectv_saved_policy_version_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
        set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
        add_subdirectory(
                "${PROJECT_SOURCE_DIR}/external/tracy/profiler"
                "${CMAKE_BINARY_DIR}/external/tracy/profiler"
                EXCLUDE_FROM_ALL
        )
        if (DEFINED projectv_saved_policy_version_minimum AND NOT projectv_saved_policy_version_minimum STREQUAL "")
            set(CMAKE_POLICY_VERSION_MINIMUM "${projectv_saved_policy_version_minimum}")
        else ()
            unset(CMAKE_POLICY_VERSION_MINIMUM)
        endif ()
        projectv_suppress_external_warnings("${PROJECT_SOURCE_DIR}/external/tracy/profiler")
    endif ()

    if (TARGET TracyClient AND MSVC)
        target_compile_options(TracyClient PRIVATE /W0)
    endif ()
endfunction()
