readline: readline.cc readline.hh
	clang++ -std=c++17 -stdlib=libc++ $? $<
