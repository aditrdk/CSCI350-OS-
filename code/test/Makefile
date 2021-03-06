# use normal make for this Makefile
#
# Makefile for building user programs to run on top of Nachos
#
# Several things to be aware of:
#
#    Nachos assumes that the location of the program startup routine (the
# 	location the kernel jumps to when the program initially starts up)
#       is at location 0.  This means: start.o must be the first .o passed 
# 	to ld, in order for the routine "Start" to be loaded at location 0
#

# If you are running on SunOS 5.x machines, uncomment the next line
GCCDIR = ../gnu/
LDFLAGS = -T newscript -N
ASFLAGS = -mips1
CPPFLAGS = $(INCDIR)

# if you aren't cross-compiling:
# GCCDIR =
# LDFLAGS = -N -T 0
# ASFLAGS =
# CPPFLAGS = -P $(INCDIR)


CC = $(GCCDIR)gcc
AS = $(GCCDIR)as
LD = $(GCCDIR)ld
CPP = $(GCCDIR)cpp

INCDIR =-I../userprog -I../threads
CFLAGS = -G 0 -c $(INCDIR)

all: halt matmult sort testfiles yieldTest lockTest threadsTest execTest conditionTest forkTest printTest networklockTest networkreleaseTest multipassportofficeTest passportoffice networkTest networkconditionTest networksignalTest networkbroadcastTest networkmonitorTest networkgetmonitorTest 2matmult customer appclerk createCustomers createPassportOffice

start.o: start.s ../userprog/syscall.h
	$(CPP) $(CPPFLAGS) start.s > strt.s
	$(AS) $(ASFLAGS) -o start.o strt.s
	rm strt.s

halt.o: halt.c
	$(CC) $(CFLAGS) -c halt.c
halt: halt.o start.o
	$(LD) $(LDFLAGS) start.o halt.o -o halt.coff
	../bin/coff2noff halt.coff halt

sort.o: sort.c
	$(CC) $(CFLAGS) -c sort.c
sort: sort.o start.o
	$(LD) $(LDFLAGS) start.o sort.o -o sort.coff
	../bin/coff2noff sort.coff sort

yieldTest.o: yieldTest.c
	$(CC) $(CFLAGS) -c yieldTest.c
yieldTest: yieldTest.o start.o
	$(LD) $(LDFLAGS) start.o yieldTest.o -o yieldTest.coff
	../bin/coff2noff yieldTest.coff yieldTest

lockTest.o: lockTest.c
	$(CC) $(CFLAGS) -c lockTest.c
lockTest: lockTest.o start.o
	$(LD) $(LDFLAGS) start.o lockTest.o -o lockTest.coff
	../bin/coff2noff lockTest.coff lockTest

threadsTest.o: threadsTest.c
	$(CC) $(CFLAGS) -c threadsTest.c
threadsTest: threadsTest.o start.o
	$(LD) $(LDFLAGS) start.o threadsTest.o -o threadsTest.coff
	../bin/coff2noff threadsTest.coff threadsTest

execTest.o: execTest.c
	$(CC) $(CFLAGS) -c execTest.c
execTest: execTest.o start.o
	$(LD) $(LDFLAGS) start.o execTest.o -o execTest.coff
	../bin/coff2noff execTest.coff execTest

conditionTest.o: conditionTest.c
	$(CC) $(CFLAGS) -c conditionTest.c
conditionTest: conditionTest.o start.o
	$(LD) $(LDFLAGS) start.o conditionTest.o -o conditionTest.coff
	../bin/coff2noff conditionTest.coff conditionTest

forkTest.o: forkTest.c
	$(CC) $(CFLAGS) -c forkTest.c
forkTest: forkTest.o start.o
	$(LD) $(LDFLAGS) start.o forkTest.o -o forkTest.coff
	../bin/coff2noff forkTest.coff forkTest

printTest.o: printTest.c
	$(CC) $(CFLAGS) -c printTest.c
printTest: printTest.o start.o
	$(LD) $(LDFLAGS) start.o printTest.o -o printTest.coff
	../bin/coff2noff printTest.coff printTest

networkTest.o: networkTest.c
	$(CC) $(CFLAGS) -c networkTest.c
networkTest: networkTest.o start.o
	$(LD) $(LDFLAGS) start.o networkTest.o -o networkTest.coff
	../bin/coff2noff networkTest.coff networkTest

networklockTest.o: networklockTest.c
	$(CC) $(CFLAGS) -c networklockTest.c
networklockTest: networklockTest.o start.o
	$(LD) $(LDFLAGS) start.o networklockTest.o -o networklockTest.coff
	../bin/coff2noff networklockTest.coff networklockTest

networkreleaseTest.o: networkreleaseTest.c
	$(CC) $(CFLAGS) -c networkreleaseTest.c
networkreleaseTest: networkreleaseTest.o start.o
	$(LD) $(LDFLAGS) start.o networkreleaseTest.o -o networkreleaseTest.coff
	../bin/coff2noff networkreleaseTest.coff networkreleaseTest

networksignalTest.o: networksignalTest.c
	$(CC) $(CFLAGS) -c networksignalTest.c
networksignalTest: networksignalTest.o start.o
	$(LD) $(LDFLAGS) start.o networksignalTest.o -o networksignalTest.coff
	../bin/coff2noff networksignalTest.coff networksignalTest

networkconditionTest.o: networkconditionTest.c
	$(CC) $(CFLAGS) -c networkconditionTest.c
networkconditionTest: networkconditionTest.o start.o
	$(LD) $(LDFLAGS) start.o networkconditionTest.o -o networkconditionTest.coff
	../bin/coff2noff networkconditionTest.coff networkconditionTest

networkbroadcastTest.o: networkbroadcastTest.c
	$(CC) $(CFLAGS) -c networkbroadcastTest.c
networkbroadcastTest: networkbroadcastTest.o start.o
	$(LD) $(LDFLAGS) start.o networkbroadcastTest.o -o networkbroadcastTest.coff
	../bin/coff2noff networkbroadcastTest.coff networkbroadcastTest

networkmonitorTest.o: networkmonitorTest.c
	$(CC) $(CFLAGS) -c networkmonitorTest.c
networkmonitorTest: networkmonitorTest.o start.o
	$(LD) $(LDFLAGS) start.o networkmonitorTest.o -o networkmonitorTest.coff
	../bin/coff2noff networkmonitorTest.coff networkmonitorTest

networkgetmonitorTest.o: networkgetmonitorTest.c
	$(CC) $(CFLAGS) -c networkgetmonitorTest.c
networkgetmonitorTest: networkgetmonitorTest.o start.o
	$(LD) $(LDFLAGS) start.o networkgetmonitorTest.o -o networkgetmonitorTest.coff
	../bin/coff2noff networkgetmonitorTest.coff networkgetmonitorTest

multipassportofficeTest.o: multipassportofficeTest.c
	$(CC) $(CFLAGS) -c multipassportofficeTest.c
multipassportofficeTest: multipassportofficeTest.o start.o
	$(LD) $(LDFLAGS) start.o multipassportofficeTest.o -o multipassportofficeTest.coff
	../bin/coff2noff multipassportofficeTest.coff multipassportofficeTest

matmult.o: matmult.c
	$(CC) $(CFLAGS) -c matmult.c
matmult: matmult.o start.o
	$(LD) $(LDFLAGS) start.o matmult.o -o matmult.coff
	../bin/coff2noff matmult.coff matmult

testfiles.o: testfiles.c
	$(CC) $(CFLAGS) -c testfiles.c
testfiles: testfiles.o start.o
	$(LD) $(LDFLAGS) start.o testfiles.o -o testfiles.coff
	../bin/coff2noff testfiles.coff testfiles

passportoffice.o: passportoffice.c
	$(CC) $(CFLAGS) -c passportoffice.c
passportoffice: passportoffice.o start.o
	$(LD) $(LDFLAGS) start.o passportoffice.o -o passportoffice.coff
	../bin/coff2noff passportoffice.coff passportoffice

2matmult.o: 2matmult.c
	$(CC) $(CFLAGS) -c 2matmult.c
2matmult: 2matmult.o start.o
	$(LD) $(LDFLAGS) start.o 2matmult.o -o 2matmult.coff
	../bin/coff2noff 2matmult.coff 2matmult

customer.o: customer.c passport.h
	$(CC) $(CFLAGS) -c customer.c
customer: customer.o start.o
	$(LD) $(LDFLAGS) start.o customer.o -o customer.coff
	../bin/coff2noff customer.coff customer

appclerk.o: appclerk.c passport.h
	$(CC) $(CFLAGS) -c appclerk.c
appclerk: appclerk.o start.o
	$(LD) $(LDFLAGS) start.o appclerk.o -o appclerk.coff
	../bin/coff2noff appclerk.coff appclerk

createCustomers.o: createCustomers.c passport.h
	$(CC) $(CFLAGS) -c createCustomers.c
createCustomers: createCustomers.o start.o
	$(LD) $(LDFLAGS) start.o createCustomers.o -o createCustomers.coff
	../bin/coff2noff createCustomers.coff createCustomers

createPassportOffice.o: createPassportOffice.c passport.h
	$(CC) $(CFLAGS) -c createPassportOffice.c
createPassportOffice: createPassportOffice.o start.o
	$(LD) $(LDFLAGS) start.o createPassportOffice.o -o createPassportOffice.coff
	../bin/coff2noff createPassportOffice.coff createPassportOffice


clean:
	rm -f *.o *.coff
