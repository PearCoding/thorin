namespace thorin {
    enum class KernelArgType : uint8_t { Val = 0, Ptr, Struct };

    static const char* runtime_definitions = R"(
        ; Module anydsl runtime decls
        declare noalias ptr @anydsl_std_allocate(i32, i64);
        declare void @anydsl_std_launch_kernel(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32);
        declare void @anydsl_std_parallel_for(i32, i32, i32, ptr, ptr);
        declare void @anydsl_fibers_spawn(i32, i32, i32, ptr, ptr);
        declare i32  @anydsl_std_spawn_thread(ptr, ptr);
        declare void @anydsl_std_sync_thread(i32);
    )";
}
