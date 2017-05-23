#/bin/sh
lcov --directory . --capture --output-file coverage.info --no-external
mkdir -p cov-report/
genhtml -o cov-report/ coverage.info

