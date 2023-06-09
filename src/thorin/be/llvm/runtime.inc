namespace thorin {
    enum class KernelArgType : uint8_t { Val = 0, Ptr, Struct };

    static const char* runtime_definitions = R"(
        ; Module anydsl runtime decls
        declare noalias [0 x i8]* @anydsl_std_allocate(i32, i64);
        declare void @anydsl_std_launch_kernel(i32, i8*, i8*, i32*, i32*, i8**, i32*, i32*, i32*, i8*, i32);
        declare void @anydsl_std_parallel_for(i32, i32, i32, i8*, i8*);
        declare void @anydsl_fibers_spawn(i32, i32, i32, i8*, i8*);
        declare i32  @anydsl_std_spawn_thread(i8*, i8*);
        declare void @anydsl_std_sync_thread(i32);
    )";
}
