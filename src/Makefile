CFLAGS = -std=gnu99 -Wall -Werror -Wextra -Wno-unused -D_POSIX_C_SOURCE="200809L" -D_DEFAULT_SOURCE -D_GNU_SOURCE -I./include/ -DHAVE_IPV6

CC = gcc

LDFLAGS = -pthread -lm -lrt -lpcre2-8 -L./lib/ -lfiredns
TESTLDFLAGS = -lcheck -lsubunit
LD = $(CC)

ifeq (${RELEASE}, y)
	CFLAGS += -O2 -flto
	LDFLAGS += -flto
else
	CFLAGS += -g3 -O0
	LDFLAGS += -g3 -O0
endif

sources = $(wildcard *.c)
testSources = $(patsubst smtpserver.c,,$(sources))
objs = $(patsubst %.c, ./obj/%.o, $(sources))

testObjs = $(patsubst %.c, ./obj/%.o, $(testSources))
testCaseObjs = $(patsubst %.c, ./obj/%.o, $(wildcard test/*.c))


server: obj_dirs server_compile

all: obj_dirs doxygen server all_tests

all_tests: server test run_all_tests

server_compile: $(objs)
	$(LD) -o smtpserver $^ $(LDFLAGS)

test: obj_dirs test_compile
	./test/servertest

test_compile:  $(testObjs) $(testCaseObjs)
	$(LD) -o test/servertest $^ $(LDFLAGS) $(TESTLDFLAGS)

	
obj/%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

test/obj/%.o: test/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -f smtpserver test/servertest ./refman.pdf ./refman.toc ./report.pdf
	rm -f ./report/utils/print_regexp ./report/report.aux ./report/report.log ./report/report.log ./report/report.synctex.gz ./report/report.toc ./report/report.out ./report/report.pdf ./report/commands.pdf ./report/report.aux ./report/report.dvi
	rm -rf obj
	rm -rf doc
	rm -rf maildir
	rm *.txt
obj_dirs:
	mkdir -p ./obj
	mkdir -p ./obj/test
	mkdir -p ./obj/report/utils
		
	
run_ipv4_system_test: server
	cd ./scenarios; python3 ./system_test.py ipv4

run_ipv6_system_test: server
	cd ./scenarios; python3 ./system_test.py ipv6
	
run_valgrind_system_test: server
	cd ./scenarios; python3 ./system_test.py valgrind
	
run_all_system_tests:  run_ipv4_system_test run_ipv6_system_test run_valgrind_system_test

run_all_tests: run_all_system_tests test

report/utils/%.o: report/utils/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

compile_print_regexp: $(testObjs) report/utils/print_regexp.c
	$(LD) -o report/utils/print_regexp $^ $(LDFLAGS)


print_regexp: obj_dirs compile_print_regexp

generate_commands: obj_dirs compile_print_regexp
	cd ./report/utils; ./print_regexp

generate_pictures: generate_commands
	cd ./report/utils; ./generate_call_graph.sh; ./generate_fsm.sh; ./generate_makefile_graph.sh
	
report: generate_commands generate_pictures doxygen
	cd ./report; TEXINPUTS=../doc/latex/:./:  pdflatex ./report.tex
	mv ./report/report.pdf ./
	

.PHONY: all
