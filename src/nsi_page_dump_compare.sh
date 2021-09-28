#!/bin/sh
#
# Name: nsi_page_dump_compare
# Author: Neeraj Jain
# Purpsose: This shell compares log and page_dump.txt files under the test run directory and gets the difference
# Usage:  
#[cavisson@cavisson-server TR39933]$ bash nsi_page_dump_compare.sh
#10a11,19
#> 1_2_2_0_0_4_4_4_0.dat
#> 1_3_3_0_0_3_3_3_0.dat
#> 1_4_4_0_0_0_0_0_0.dat
#> 1_5_5_0_0_7_7_7_0.dat
#> 1_6_6_0_0_7_7_7_0.dat
#> 1_7_7_0_0_6_6_6_0.dat
#> 1_8_8_0_0_6_6_6_0.dat
#> 1_9_9_0_0_6_6_6_0.dat
#> 1_10_10_0_0_2_2_2_0.dat
#24a34,35
#> 2_2_2_0_0_7_7_7_0.dat
#> 2_3_3_0_0_2_2_2_0.dat

cat page_dump.txt | grep -v "^StartTime" | cut -f 12 -d '|' | cut -c 9- > page_dump.req
grep -a FileSuffix log | cut -f 12 -d '=' | cut -f 1 -d ';' > log.req

diff page_dump.req log.req

