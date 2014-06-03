#!/bin/bash
lista=`find . -name "*.log"`
echo "$lista"
find . -name "*.log" -print0 | xargs -0 rm -rf
echo "Logs borrados correctamente"
