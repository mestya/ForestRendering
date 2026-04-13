# FixOldCMake.cmake

function(force_update_cmake_version search_dir)
    message(STATUS "Scanning for legacy CMakeLists.txt in: ${search_dir}")
    
    # 递归查找目录下所有的 CMakeLists.txt
    file(GLOB_RECURSE ALL_CMAKELISTS "${search_dir}/CMakeLists.txt")
    
    foreach(FILE_PATH ${ALL_CMAKELISTS})
        file(READ ${FILE_PATH} CONTENT)
        
        # 检查是否包含 cmake_minimum_required
        if(CONTENT MATCHES "cmake_minimum_required")
            # 使用正则表达式匹配版本号部分，并替换为 3.10
            # 兼容 VERSION 2.8, VERSION "2.8", VERSION 3.0 等写法
            string(REGEX REPLACE 
                "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+(\\.[0-9]+)?\\)" 
                "cmake_minimum_required(VERSION 3.10)" 
                NEW_CONTENT "${CONTENT}")
            
            # 如果内容有变动，写回文件
            if(NOT "${CONTENT}" STREQUAL "${NEW_CONTENT}")
                message(STATUS "Fixed: ${FILE_PATH}")
                file(WRITE ${FILE_PATH} "${NEW_CONTENT}")
            endif()
        endif()
    endforeach()
endfunction()