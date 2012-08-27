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
					
				],
				'cflags_cc': [
					'-fexceptions',
					'-Wall',
					'-O3'
				],
			},

	]
}
