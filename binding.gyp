{
    "targets": [
        {
            "target_name": "testtools-observer",

            "sources": [
                "src/abstractobserver.h",
                "src/processobserver.h",
                "src/systemobserver.h",
                "src/observer.cc",
                "src/observer.h",
            ],

            "cflags_cc!": [
                "-fno-exceptions"
            ],

            "cflags_cc+": [
                "-fexceptions",
                "-std=c++11"
            ],

            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ],

            "conditions": [
                ["OS == 'win'",
                    {
                        "defines": [
                            "TESTTOOLS_WIN"
                        ],

                        "sources": [
                            "src/abstractobserver_win.cc",
                            "src/processobserver_win.cc",
                            "src/systemobserver_win.cc"
                        ],

                        "msvs_settings": {
                            "VCCLCompilerTool": {
                                "WarningLevel": 3,
                                "ExceptionHandling": 2  # /EHsc
                            },
                        }
                    }
                 ],

                ["OS == 'linux'",
                    {
                        "cflags_cc+": [
                            "-Wno-missing-field-initializers",
                            "-Wno-missing-braces",
                            "-Wno-unused-result"
                        ],

                        "defines": [
                            "TESTTOOLS_LINUX"
                        ],

                        "sources": [
                            "src/abstractobserver_linux.cc",
                            "src/processobserver_linux.cc",
                            "src/systemobserver_linux.cc"
                        ]
                    }
                 ]
            ]
        }
    ]
}
