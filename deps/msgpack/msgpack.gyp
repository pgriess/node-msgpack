{
	"targets": [
		{
				'target_name': 'libmsgpack',
				'include_dirs': [ '.' ],
				'type': 'static_library',
				'sources': [
					'object.cpp',
					'objectc.c',
					'unpack.c',
					'vrefbuffer.c',
					'zone.c',
					'version.c'					
				],
				'cflags_cc': [
					'-Wall',
					'-O3'
				],
				'cflags': [
					'-Wall',
					'-O3'
				],
				'cflags!': ['-fno-exceptions'],
				'cflags_cc!': ['-fno-exceptions'],
				'conditions': [
					['OS=="mac"', {
						'xcode_settings': {
							'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
						}

					}],
					['OS=="win"', {
						'configurations': {
							'Debug': {
								'msvs_settings': {
									'VCCLCompilerTool': {
										'CompileAs': '2',
										'ExceptionHandling': '1',
									},
								},
							},
							'Release': {
								'msvs_settings': {
									'VCCLCompilerTool': {
										'CompileAs': '2',
										'ExceptionHandling': '1',
									},
								},
							},
						},
					}]
				]
			},
	]
}
