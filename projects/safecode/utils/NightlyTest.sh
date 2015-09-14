#!/bin/sh

# Location of the LLVM source and object trees
LLVMDIR=$HOME/cronjobs/llvm27

# Location of test suite object tree
TESTSUITE=$LLVMDIR/projects/test-suite

# Location of file containing log of the test build and run
LOGFILE=$LLVMDIR/projects/safecode/test/results

# List of directories to clean before test
TESTDIRS="MultiSource/Benchmarks/Olden External/SPEC/CINT2000"

#
# Save the old logfile.
#
if [ -f $LOGFILE ]
then
  mv $LOGFILE $LOGFILE.old
fi

#
# Switch to the LLVM source tree.
#
cd $LLVMDIR

#
# Make sure LLVM is up-to-date.
#
echo "Updating LLVM"
svn up
echo "Building LLVM"
make -s -j3 tools-only > $LOGFILE

#
# Update and build Automatic Pool Allocation
#
cd $LLVMDIR/projects/poolalloc
echo "Updating Poolalloc"
svn up
echo "Building Poolalloc"
make -s -j3 >> $LOGFILE

#
# Update and build SAFECode
#
cd $LLVMDIR/projects/safecode
echo "Updating SAFECode"
svn up
echo "Building SAFECode"
make -s -j3 >> $LOGFILE

#
# Clean out the old test files.
#
echo "Cleaning out old test files..."
cd $TESTSUITE
make -k clean >> $LOGFILE

#
# Run the automatic pool allocation tests.
#
echo "Testing SAFECode..."
cd $LLVMDIR/projects/safecode/test
make -k lit 2>&1 | tee -a $LOGFILE
make NO_STABLE_NUMBERS=1 NO_LARGE_SIZE=1 -k -j3 progdebug 2>&1 >> $LOGFILE

#
# Print out the results.
#
echo "Logfile is in $LOGFILE"
for dir in $TESTDIRS
do
  cat $TESTSUITE/$dir/report.debug.txt
done

