#!/bin/bash

set -x -e

./compile.sh

iverilog-vpi vpi_axi_master.c

gcc -Wall -Werror axi_master_client.c -o axi_master_client

