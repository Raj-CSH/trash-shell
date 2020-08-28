CC=clang
BIN_DIR=bin
SRC_DIR=src
INCLUDE_DIR=include
CFLAGS= -I${INCLUDE_DIR} -c
CCFLAGS = -lreadline
OBJS = ${BIN_DIR}/main.o ${BIN_DIR}/utils.o ${BIN_DIR}/interpreter.o ${BIN_DIR}/builtins.o

ash: ${OBJS}
	${CC} ${OBJS} ${CCFLAGS} -o ${BIN_DIR}/trash

${BIN_DIR}/main.o: ${SRC_DIR}/main.c ${INCLUDE_DIR}/trash/*.h
	${CC} ${SRC_DIR}/main.c ${CFLAGS} -o ${BIN_DIR}/main.o

${BIN_DIR}/utils.o: ${SRC_DIR}/utils.c ${INCLUDE_DIR}/trash/utils.h
	${CC} ${SRC_DIR}/utils.c ${CFLAGS} -o ${BIN_DIR}/utils.o

${BIN_DIR}/interpreter.o: ${SRC_DIR}/interpreter.c ${INCLUDE_DIR}/trash/*.h
	${CC} ${SRC_DIR}/interpreter.c ${CFLAGS} -o ${BIN_DIR}/interpreter.o

${BIN_DIR}/builtins.o: ${SRC_DIR}/builtins.c ${INCLUDE_DIR}/trash/*.h
	${CC} ${SRC_DIR}/builtins.c ${CFLAGS} -o ${BIN_DIR}/builtins.o

.PHONY : format
format:
	clang-format -i ${INCLUDE_DIR}/trash/*.h ${SRC_DIR}/*.c

.PHONY : clean
clean:
	rm bin/*
