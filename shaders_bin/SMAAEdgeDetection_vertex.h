
static const uint8_t SMAAEdgeDetection_vertex_gl[672] =
{
	0x56, 0x53, 0x48, 0x06, 0x00, 0x00, 0x00, 0x00, 0x4c, 0xe6, 0x73, 0x2e, 0x02, 0x00, 0x0f, 0x75, // VSH.....L.s....u
	0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x04, 0x01, // _modelViewProj..
	0x00, 0x00, 0x01, 0x00, 0x0d, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, 0x69, // .....u_SmaaMetri
	0x63, 0x73, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x63, 0x02, 0x00, 0x00, 0x69, 0x6e, 0x20, 0x76, // cs......c...in v
	0x65, 0x63, 0x33, 0x20, 0x61, 0x5f, 0x70, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x3b, 0x0a, // ec3 a_position;.
	0x69, 0x6e, 0x20, 0x76, 0x65, 0x63, 0x34, 0x20, 0x61, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, // in vec4 a_texcoo
	0x72, 0x64, 0x30, 0x3b, 0x0a, 0x6f, 0x75, 0x74, 0x20, 0x76, 0x65, 0x63, 0x32, 0x20, 0x76, 0x5f, // rd0;.out vec2 v_
	0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x30, 0x3b, 0x0a, 0x6f, 0x75, 0x74, 0x20, 0x76, // texcoord0;.out v
	0x65, 0x63, 0x34, 0x20, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x32, 0x3b, // ec4 v_texcoord2;
	0x0a, 0x6f, 0x75, 0x74, 0x20, 0x76, 0x65, 0x63, 0x34, 0x20, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, // .out vec4 v_texc
	0x6f, 0x6f, 0x72, 0x64, 0x33, 0x3b, 0x0a, 0x6f, 0x75, 0x74, 0x20, 0x76, 0x65, 0x63, 0x34, 0x20, // oord3;.out vec4 
	0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x34, 0x3b, 0x0a, 0x75, 0x6e, 0x69, // v_texcoord4;.uni
	0x66, 0x6f, 0x72, 0x6d, 0x20, 0x6d, 0x61, 0x74, 0x34, 0x20, 0x75, 0x5f, 0x6d, 0x6f, 0x64, 0x65, // form mat4 u_mode
	0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x3b, 0x0a, 0x75, 0x6e, 0x69, 0x66, 0x6f, // lViewProj;.unifo
	0x72, 0x6d, 0x20, 0x76, 0x65, 0x63, 0x34, 0x20, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, // rm vec4 u_SmaaMe
	0x74, 0x72, 0x69, 0x63, 0x73, 0x3b, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x6d, 0x61, 0x69, 0x6e, // trics;.void main
	0x20, 0x28, 0x29, 0x0a, 0x7b, 0x0a, 0x20, 0x20, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, //  ().{.  v_texcoo
	0x72, 0x64, 0x30, 0x20, 0x3d, 0x20, 0x61, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, // rd0 = a_texcoord
	0x30, 0x2e, 0x78, 0x79, 0x3b, 0x0a, 0x20, 0x20, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, // 0.xy;.  v_texcoo
	0x72, 0x64, 0x32, 0x20, 0x3d, 0x20, 0x28, 0x28, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, // rd2 = ((u_SmaaMe
	0x74, 0x72, 0x69, 0x63, 0x73, 0x2e, 0x78, 0x79, 0x78, 0x79, 0x20, 0x2a, 0x20, 0x76, 0x65, 0x63, // trics.xyxy * vec
	0x34, 0x28, 0x2d, 0x31, 0x2e, 0x30, 0x2c, 0x20, 0x30, 0x2e, 0x30, 0x2c, 0x20, 0x30, 0x2e, 0x30, // 4(-1.0, 0.0, 0.0
	0x2c, 0x20, 0x2d, 0x31, 0x2e, 0x30, 0x29, 0x29, 0x20, 0x2b, 0x20, 0x61, 0x5f, 0x74, 0x65, 0x78, // , -1.0)) + a_tex
	0x63, 0x6f, 0x6f, 0x72, 0x64, 0x30, 0x2e, 0x78, 0x79, 0x78, 0x79, 0x29, 0x3b, 0x0a, 0x20, 0x20, // coord0.xyxy);.  
	0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x33, 0x20, 0x3d, 0x20, 0x28, 0x28, // v_texcoord3 = ((
	0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, 0x69, 0x63, 0x73, 0x2e, 0x78, 0x79, // u_SmaaMetrics.xy
	0x78, 0x79, 0x20, 0x2a, 0x20, 0x76, 0x65, 0x63, 0x34, 0x28, 0x31, 0x2e, 0x30, 0x2c, 0x20, 0x30, // xy * vec4(1.0, 0
	0x2e, 0x30, 0x2c, 0x20, 0x30, 0x2e, 0x30, 0x2c, 0x20, 0x31, 0x2e, 0x30, 0x29, 0x29, 0x20, 0x2b, // .0, 0.0, 1.0)) +
	0x20, 0x61, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x30, 0x2e, 0x78, 0x79, 0x78, //  a_texcoord0.xyx
	0x79, 0x29, 0x3b, 0x0a, 0x20, 0x20, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, // y);.  v_texcoord
	0x34, 0x20, 0x3d, 0x20, 0x28, 0x28, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, // 4 = ((u_SmaaMetr
	0x69, 0x63, 0x73, 0x2e, 0x78, 0x79, 0x78, 0x79, 0x20, 0x2a, 0x20, 0x76, 0x65, 0x63, 0x34, 0x28, // ics.xyxy * vec4(
	0x2d, 0x32, 0x2e, 0x30, 0x2c, 0x20, 0x30, 0x2e, 0x30, 0x2c, 0x20, 0x30, 0x2e, 0x30, 0x2c, 0x20, // -2.0, 0.0, 0.0, 
	0x2d, 0x32, 0x2e, 0x30, 0x29, 0x29, 0x20, 0x2b, 0x20, 0x61, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, // -2.0)) + a_texco
	0x6f, 0x72, 0x64, 0x30, 0x2e, 0x78, 0x79, 0x78, 0x79, 0x29, 0x3b, 0x0a, 0x20, 0x20, 0x76, 0x65, // ord0.xyxy);.  ve
	0x63, 0x34, 0x20, 0x74, 0x6d, 0x70, 0x76, 0x61, 0x72, 0x5f, 0x31, 0x3b, 0x0a, 0x20, 0x20, 0x74, // c4 tmpvar_1;.  t
	0x6d, 0x70, 0x76, 0x61, 0x72, 0x5f, 0x31, 0x2e, 0x77, 0x20, 0x3d, 0x20, 0x31, 0x2e, 0x30, 0x3b, // mpvar_1.w = 1.0;
	0x0a, 0x20, 0x20, 0x74, 0x6d, 0x70, 0x76, 0x61, 0x72, 0x5f, 0x31, 0x2e, 0x78, 0x79, 0x7a, 0x20, // .  tmpvar_1.xyz 
	0x3d, 0x20, 0x61, 0x5f, 0x70, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x3b, 0x0a, 0x20, 0x20, // = a_position;.  
	0x67, 0x6c, 0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x3d, 0x20, 0x28, 0x75, // gl_Position = (u
	0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x20, 0x2a, // _modelViewProj *
	0x20, 0x74, 0x6d, 0x70, 0x76, 0x61, 0x72, 0x5f, 0x31, 0x29, 0x3b, 0x0a, 0x7d, 0x0a, 0x0a, 0x00, //  tmpvar_1);.}...
};

static const uint8_t SMAAEdgeDetection_vertex_d3d11[812] =
{
	0x56, 0x53, 0x48, 0x06, 0x00, 0x00, 0x00, 0x00, 0x4c, 0xe6, 0x73, 0x2e, 0x02, 0x00, 0x0f, 0x75, // VSH.....L.s....u
	0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x04, 0x00, // _modelViewProj..
	0x00, 0x00, 0x04, 0x00, 0x0d, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, 0x69, // .....u_SmaaMetri
	0x63, 0x73, 0x02, 0x00, 0x40, 0x00, 0x01, 0x00, 0xe8, 0x02, 0x00, 0x00, 0x44, 0x58, 0x42, 0x43, // cs..@.......DXBC
	0xe2, 0xfe, 0x84, 0x62, 0x6a, 0x9c, 0x2a, 0x1e, 0x22, 0xe8, 0x4a, 0x52, 0x51, 0x31, 0x28, 0x74, // ...bj.*.".JRQ1(t
	0x01, 0x00, 0x00, 0x00, 0xe8, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, // ............,...
	0x80, 0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 0x00, 0x49, 0x53, 0x47, 0x4e, 0x4c, 0x00, 0x00, 0x00, // .... ...ISGNL...
	0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........8.......
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00, // ................
	0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, // A...............
	0x01, 0x00, 0x00, 0x00, 0x0f, 0x03, 0x00, 0x00, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, // ........POSITION
	0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0xab, 0xab, 0x4f, 0x53, 0x47, 0x4e, // .TEXCOORD...OSGN
	0x98, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, // ................
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x0f, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, // ................
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, // ................
	0x0f, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, // ................
	0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, // ................
	0x0f, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00, // ....SV_POSITION.
	0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0xab, 0xab, 0xab, 0x53, 0x48, 0x45, 0x58, // TEXCOORD....SHEX
	0xc0, 0x01, 0x00, 0x00, 0x50, 0x00, 0x01, 0x00, 0x70, 0x00, 0x00, 0x00, 0x6a, 0x08, 0x00, 0x01, // ....P...p...j...
	0x59, 0x00, 0x00, 0x04, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, // Y...F. .........
	0x5f, 0x00, 0x00, 0x03, 0x72, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x00, 0x00, 0x03, // _...r......._...
	0x32, 0x10, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x04, 0xf2, 0x20, 0x10, 0x00, // 2.......g.... ..
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, 0x32, 0x20, 0x10, 0x00, // ........e...2 ..
	0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, 0xf2, 0x20, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, // ....e.... ......
	0x65, 0x00, 0x00, 0x03, 0xf2, 0x20, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, // e.... ......e...
	0xf2, 0x20, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, // . ......h.......
	0x38, 0x00, 0x00, 0x08, 0xf2, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x15, 0x10, 0x00, // 8...........V...
	0x00, 0x00, 0x00, 0x00, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // ....F. .........
	0x32, 0x00, 0x00, 0x0a, 0xf2, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x8e, 0x20, 0x00, // 2...........F. .
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x46, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a, 0xf2, 0x00, 0x10, 0x00, // F.......2.......
	0x00, 0x00, 0x00, 0x00, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, // ....F. .........
	0xa6, 0x1a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // ........F.......
	0x00, 0x00, 0x00, 0x08, 0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x0e, 0x10, 0x00, // ..... ......F...
	0x00, 0x00, 0x00, 0x00, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, // ....F. .........
	0x36, 0x00, 0x00, 0x05, 0x32, 0x20, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x46, 0x10, 0x10, 0x00, // 6...2 ......F...
	0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0d, 0xf2, 0x20, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, // ....2.... ......
	0x46, 0x84, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, // F. ..........@..
	0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, // ................
	0x46, 0x14, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0d, 0xf2, 0x20, 0x10, 0x00, // F.......2.... ..
	0x03, 0x00, 0x00, 0x00, 0x46, 0x84, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, // ....F. .........
	0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // .@.....?........
	0x00, 0x00, 0x80, 0x3f, 0x46, 0x14, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0d, // ...?F.......2...
	0xf2, 0x20, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x46, 0x84, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, // . ......F. .....
	0x04, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, // .....@..........
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x46, 0x14, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, // ........F.......
	0x3e, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x00, 0x10, 0x00, 0x50, 0x00,                         // >.........P.
};

static const uint8_t SMAAEdgeDetection_vertex_vk[2104] =
{
	0x56, 0x53, 0x48, 0x06, 0x00, 0x00, 0x00, 0x00, 0x4c, 0xe6, 0x73, 0x2e, 0x02, 0x00, 0x0f, 0x75, // VSH.....L.s....u
	0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x04, 0x01, // _modelViewProj..
	0x00, 0x00, 0x04, 0x00, 0x0d, 0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, 0x69, // .....u_SmaaMetri
	0x63, 0x73, 0x02, 0x01, 0x40, 0x00, 0x01, 0x00, 0xf4, 0x07, 0x00, 0x00, 0x03, 0x02, 0x23, 0x07, // cs..@.........#.
	0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x08, 0x00, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, // ................
	0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00, // GLSL.std.450....
	0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x0c, 0x00, // ................
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, // ........main....
	0x73, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, // s...w...........
	0x87, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, // ................
	0x05, 0x00, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, // ................
	0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x1f, 0x00, 0x00, 0x00, // main............
	0x24, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x00, 0x06, 0x00, 0x07, 0x00, 0x1f, 0x00, 0x00, 0x00, // $Global.........
	0x00, 0x00, 0x00, 0x00, 0x75, 0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, // ....u_modelViewP
	0x72, 0x6f, 0x6a, 0x00, 0x06, 0x00, 0x07, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // roj.............
	0x75, 0x5f, 0x53, 0x6d, 0x61, 0x61, 0x4d, 0x65, 0x74, 0x72, 0x69, 0x63, 0x73, 0x00, 0x00, 0x00, // u_SmaaMetrics...
	0x05, 0x00, 0x03, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, // ....!...........
	0x73, 0x00, 0x00, 0x00, 0x61, 0x5f, 0x70, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, // s...a_position..
	0x05, 0x00, 0x05, 0x00, 0x77, 0x00, 0x00, 0x00, 0x61, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, // ....w...a_texcoo
	0x72, 0x64, 0x30, 0x00, 0x05, 0x00, 0x0a, 0x00, 0x80, 0x00, 0x00, 0x00, 0x40, 0x65, 0x6e, 0x74, // rd0.........@ent
	0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x4f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x67, 0x6c, // ryPointOutput.gl
	0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0a, 0x00, // _Position.......
	0x84, 0x00, 0x00, 0x00, 0x40, 0x65, 0x6e, 0x74, 0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x4f, // ....@entryPointO
	0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, // utput.v_texcoord
	0x30, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0a, 0x00, 0x87, 0x00, 0x00, 0x00, 0x40, 0x65, 0x6e, 0x74, // 0...........@ent
	0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x4f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x76, 0x5f, // ryPointOutput.v_
	0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x32, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0a, 0x00, // texcoord2.......
	0x8a, 0x00, 0x00, 0x00, 0x40, 0x65, 0x6e, 0x74, 0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x4f, // ....@entryPointO
	0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x76, 0x5f, 0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, // utput.v_texcoord
	0x33, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0a, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x40, 0x65, 0x6e, 0x74, // 3...........@ent
	0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x4f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x2e, 0x76, 0x5f, // ryPointOutput.v_
	0x74, 0x65, 0x78, 0x63, 0x6f, 0x6f, 0x72, 0x64, 0x34, 0x00, 0x00, 0x00, 0x48, 0x00, 0x04, 0x00, // texcoord4...H...
	0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, // ............H...
	0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........#.......
	0x48, 0x00, 0x05, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, // H...............
	0x10, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // ....H...........
	0x23, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x1f, 0x00, 0x00, 0x00, // #...@...G.......
	0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, // ....G...!..."...
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, // ....G...!...!...
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x73, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...s.......
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x77, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...w.......
	0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, // ....G...........
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x84, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...........
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x87, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...........
	0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...........
	0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ....G...........
	0x03, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, // ............!...
	0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, // ................
	0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, //  ...............
	0x02, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, // ................
	0x04, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, // ................
	0x03, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, // ............ ...
	0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, // ....+...........
	0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x04, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, // ................
	0x04, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x04, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, // ................
	0x09, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, // .... ... .......
	0x1f, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x20, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, // ....;... ...!...
	0x02, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, // ....+......."...
	0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x23, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, // .... ...#.......
	0x09, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, // ....+.......'...
	0x00, 0x00, 0x80, 0xbf, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, // ....+.......(...
	0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, // ....,.......)...
	0x27, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, // '...(...(...'...
	0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, // +.......1......?
	0x2c, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, // ,.......2...1...
	0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, // (...(...1...+...
	0x06, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x2c, 0x00, 0x07, 0x00, // ....;.......,...
	0x09, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, // ....<...;...(...
	0x28, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x63, 0x00, 0x00, 0x00, // (...;... ...c...
	0x02, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x72, 0x00, 0x00, 0x00, // ........ ...r...
	0x01, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x72, 0x00, 0x00, 0x00, // ........;...r...
	0x73, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x76, 0x00, 0x00, 0x00, // s....... ...v...
	0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x76, 0x00, 0x00, 0x00, // ........;...v...
	0x77, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x7f, 0x00, 0x00, 0x00, // w....... .......
	0x03, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x7f, 0x00, 0x00, 0x00, // ........;.......
	0x80, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x83, 0x00, 0x00, 0x00, // ........ .......
	0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x83, 0x00, 0x00, 0x00, // ........;.......
	0x84, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x7f, 0x00, 0x00, 0x00, // ........;.......
	0x87, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x7f, 0x00, 0x00, 0x00, // ........;.......
	0x8a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x7f, 0x00, 0x00, 0x00, // ........;.......
	0x8d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x03, 0x00, 0x07, 0x00, 0x00, 0x00, // ................
	0x17, 0x01, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, // ....6...........
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, // ................
	0x3d, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00, // =.......t...s...
	0x3d, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, // =.......x...w...
	0x41, 0x00, 0x05, 0x00, 0x23, 0x00, 0x00, 0x00, 0xcb, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, // A...#.......!...
	0x22, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0xcc, 0x00, 0x00, 0x00, // "...=...........
	0xcb, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00, // ....O...........
	0xcc, 0x00, 0x00, 0x00, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // ................
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x00, 0x00, // ........O.......
	0xcf, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ....x...........
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x00, // ................
	0x09, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, // ............2...
	0xcd, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0xcf, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, // ....).......A...
	0x23, 0x00, 0x00, 0x00, 0xd2, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, // #.......!..."...
	0x3d, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0xd3, 0x00, 0x00, 0x00, 0xd2, 0x00, 0x00, 0x00, // =...............
	0x4f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0x00, 0xd3, 0x00, 0x00, 0x00, // O...............
	0xd3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x01, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x00, 0x00, 0xd6, 0x00, 0x00, 0x00, // ....O...........
	0x78, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // x...............
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x09, 0x00, 0x00, 0x00, // ................
	0xd7, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0x00, // ........2.......
	0x32, 0x00, 0x00, 0x00, 0xd6, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x23, 0x00, 0x00, 0x00, // 2.......A...#...
	0xd9, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, // ....!..."...=...
	0x09, 0x00, 0x00, 0x00, 0xda, 0x00, 0x00, 0x00, 0xd9, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x09, 0x00, // ............O...
	0x09, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00, 0xda, 0x00, 0x00, 0x00, 0xda, 0x00, 0x00, 0x00, // ................
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // ................
	0x4f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x00, 0x00, 0xdd, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, // O...........x...
	0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ................
	0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x09, 0x00, 0x00, 0x00, 0xde, 0x00, 0x00, 0x00, // ................
	0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, // ....2.......<...
	0xdd, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x00, 0x00, // ....O...........
	0x78, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // x...x...........
	0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, // Q...........t...
	0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x00, // ....Q...........
	0x74, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, // t.......Q.......
	0xc0, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, // ....t.......P...
	0x09, 0x00, 0x00, 0x00, 0xc1, 0x00, 0x00, 0x00, 0xbe, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x00, // ................
	0xc0, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x63, 0x00, 0x00, 0x00, // ....1...A...c...
	0xc2, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, // ....!.......=...
	0x1e, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x00, 0x00, 0xc2, 0x00, 0x00, 0x00, 0x90, 0x00, 0x05, 0x00, // ................
	0x09, 0x00, 0x00, 0x00, 0xc4, 0x00, 0x00, 0x00, 0xc1, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x00, 0x00, // ................
	0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x00, 0x00, 0xc4, 0x00, 0x00, 0x00, // Q...............
	0x01, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00, // ................
	0xc7, 0x00, 0x00, 0x00, 0x52, 0x00, 0x06, 0x00, 0x09, 0x00, 0x00, 0x00, 0x16, 0x01, 0x00, 0x00, // ....R...........
	0xc8, 0x00, 0x00, 0x00, 0xc4, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, // ............>...
	0x80, 0x00, 0x00, 0x00, 0x16, 0x01, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x84, 0x00, 0x00, 0x00, // ........>.......
	0xb2, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x87, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, // ....>...........
	0x3e, 0x00, 0x03, 0x00, 0x8a, 0x00, 0x00, 0x00, 0xd7, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, // >...........>...
	0x8d, 0x00, 0x00, 0x00, 0xde, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00, // ............8...
	0x00, 0x02, 0x01, 0x00, 0x10, 0x00, 0x50, 0x00,                                                 // ......P.
};
