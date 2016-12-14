static unsigned int rlut[256] = {
	0xa34aafe9, 0xa7d94799, 0x5df26d1f, 0xcb0a847b, 0x57420cd3, 0x0828210e, 0xe77d1e3c, 0x6817c00c,
	0xc6f4c61b, 0x33041092, 0x135e1424, 0xfe9336bb, 0x18f57bf1, 0xc0fb9d6c, 0x87b471c9, 0x8bf2eab4,
	0x43f2478c, 0x020c8441, 0x12e46d93, 0xfb624c9a, 0x64772108, 0x6cae8cea, 0xdba42144, 0x422d5249,
	0x031671b3, 0xd73562a9, 0x03d713da, 0xbed0c527, 0x81fd321f, 0x712ec1f1, 0x1ec6084a, 0x7a62f574,
	0x29be07e2, 0x06ed81d8, 0x6ea071c0, 0x5f930e98, 0xe4eb0756, 0x6e614a14, 0x86589bee, 0xd97ea17b,
	0xb5e1667a, 0x465be0a3, 0x11e7ed84, 0xdd453f00, 0xb3b12923, 0xde6ce310, 0x9555f6a7, 0x280ecff5,
	0xb37f9463, 0x2b742466, 0x12f9851a, 0xc30354d6, 0x4edbd34a, 0xec85c35c, 0x49491e3b, 0x3c2c3a72,
	0x0a223a60, 0xe808a7cc, 0x952cefd5, 0xbd7bde6c, 0x22b67b5f, 0x1f482211, 0x5d79de80, 0x3b8870ce,
	0xe5353da2, 0x9cc1bd3d, 0xe212856e, 0x71408b2e, 0xee3c1b56, 0xd7cfea09, 0x393519fa, 0x45d2abd3,
	0xb4dafb01, 0xd76a5928, 0x3a1292e9, 0xb0ba66b2, 0x7fd6420d, 0x2be45386, 0xb5528bd9, 0xb135e550,
	0x5165cd0c, 0xf7e990cb, 0x6a03282d, 0x8ac64ea4, 0x81a45202, 0x33e69f21, 0x61a86191, 0x97e9e969,
	0xa08efe57, 0x7a05da58, 0xb44bebec, 0x437678fe, 0xd3ed9c42, 0x97f2e84d, 0xe3203272, 0xc9ac4bad,
	0x3f8312f9, 0x94a5f091, 0xd7fef670, 0xd5398f54, 0x66fae036, 0x5635ce9b, 0x66f5a715, 0x16d313b0,
	0x7bc5560d, 0x6c022389, 0x2265ffa9, 0x7c74676a, 0xe58d0daf, 0x09ae4378, 0xd8522986, 0x1d676fa3,
	0xc18cdfae, 0x911f2fae, 0xa9684614, 0x1c6c51d6, 0xc3f151cf, 0xff4b9641, 0x09192386, 0x5a097c97,
	0x528e0809, 0x73936a8e, 0xf72d9298, 0x4635ea9e, 0x9a9f8738, 0x584cf6d2, 0xe05cce65, 0x9ea63933,
	0x1d0f10b5, 0x84d412e1, 0x41ae2eda, 0x9df3e6f6, 0xd133cd8c, 0xfe5741c8, 0x18df9cdb, 0x505dfe17,
	0x834ccca6, 0x758404d3, 0xf3b1b8e9, 0xd7e1081f, 0xca805889, 0xdbd4c556, 0xe52202c6, 0xfeeb3b08,
	0x5ac439c2, 0xde0d9ac6, 0x1489f8e0, 0x673babe8, 0x3b334231, 0x378a030b, 0x4079d377, 0x42cd07fe,
	0x7bc5a72d, 0x8c4ab8fe, 0xd8d66b7c, 0xb1333301, 0x0da70967, 0x520de3ac, 0xa39ca2d3, 0x71efa58e,
	0xf5f44f7d, 0xd06062e4, 0xa68943b7, 0xa9927e46, 0xbc7b1bdb, 0xf457528a, 0x8bb29fcc, 0xe44a0348,
	0xccf4d77a, 0x70c235be, 0x1576ea68, 0x9ab38a12, 0x3cef619e, 0xee876745, 0xf4221236, 0x68534fd3,
	0x80197d78, 0x577d743c, 0x981a11d6, 0x77983709, 0x21ec9f44, 0xdb11fad8, 0xd42e2f81, 0x3a727689,
	0x080c6ce7, 0x48137fb1, 0x3ac82a75, 0x18c429de, 0x6398edb8, 0xa0874d1c, 0x494a5b27, 0x8d2cb185,
	0x89c8c6f4, 0x1d70b97c, 0xdf5262d7, 0xc307b26c, 0xd4cdb6f4, 0x6cae5a4b, 0xd011a323, 0x566c9762,
	0xbe741574, 0xb209d0cf, 0x4f7d437f, 0xeab8ff71, 0x13bf2ed3, 0x3d4fba1b, 0x8a21fe91, 0x4c9d6608,
	0x31005f55, 0x45f1f9a4, 0x494b7939, 0xbc87195a, 0x8f7393c6, 0x8d85a67b, 0x643fdefb, 0x1ccac984,
	0x5d11bde2, 0x3b698c3e, 0xdad34b5d, 0xc221b63e, 0xdd7e3440, 0x9c60ceae, 0x76bd3e32, 0x22ecf50b,
	0xc6cf5523, 0x297c66d6, 0x21e8fb89, 0xd019fc5b, 0x41a6785a, 0x0ef43158, 0x6bb02b18, 0xd054cab3,
	0x25aa0823, 0x69b72286, 0x596daa60, 0x8412c1a9, 0x7b6572d7, 0xe2c7b7b4, 0x84a59a12, 0xe977f7d0,
	0xed46b0b2, 0xdd1e6364, 0xc62a6155, 0xcb037fa9, 0x7c8be471, 0x21c11276, 0xccffebea, 0x618f8290,
	0xf4ca14a6, 0xc449e5f1, 0xe983c142, 0x356cd555, 0xf7210d04, 0x3bef636d, 0xda1388a5, 0x467c5137
};
