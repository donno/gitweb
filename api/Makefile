# For Alpine Linux:
#   apk add libgit2-dev
# For Ubuntu based system:
#   apt install libgit2-dev

CXXFLAGS=--std=c++1y
LDLIBS=-lgit2

gitjson: jsonwriter.o repository.o router.o gitjson.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

repository.o: /usr/include/git2.h
gitjson.o: /usr/include/git2.h

/usr/include/git2.h:
	@# This will require root and will only work for Alpine.
	[ -f /etc/alpine-release ] && apk add libgit2-dev
	@[ ! -f /etc/alpine-release ] && echo libgit2 headers not found.
