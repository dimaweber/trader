#/bin/sh
lcov --directory . --capture --output-file coverage.info
mkdir -p i../cov-report/
genhtml -o ../cov-report/ coverage.info

