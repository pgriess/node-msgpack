{
	"targets": [
		{
			'target_name': 'msgpackBinding',
			'sources': [
				'src/msgpack.cc',
			],
			'include_dirs': [
				'deps/msgpack'
			],
			'dependencies': [
				'deps/msgpack/msgpack.gyp:libmsgpack'
			],
			'cflags_cc': [
				'-fexceptions',
				'-Wall',
				'-O3'
				],
			},

	]
}
