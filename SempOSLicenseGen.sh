#!/bin/bash

expend="0000"
privKey="002145"
sourceFile=$1

if [[ $# != 1 || ! -a $sourceFile ]]; then
echo "usage : SempOSLicenseGen.sh <file>"
echo
exit
fi

#
#   create license file
#
licenseFile=$sourceFile".license"
echo "#port license" > $licenseFile
echo >> $licenseFile

file_row_count=`wc -l $sourceFile | awk '{print $1}'`

i=0
while [ $i -le $file_row_count ]
do
    i=$((${i}+1))
    s1=`sed -n "${i}p" "${sourceFile}"`
    
    if [[ -z s1 ]]; then
    	continue
    fi
    
    echo $s1
    pos=`echo ${s1%%/*}|sed 's/[^0-9,.:A-Za-z]//g'`
    tmp=`echo ${s1#*.}|sed 's/[^0-9,.:A-Za-z]//g'`
    if [[ -z tmp ]]; then
        tmp=`echo ${s1#*/}|sed 's/[^0-9,.:A-Za-z]//g'`
        port=`echo ${tmp%%:*}|sed 's/[^0-9,.:A-Za-z]//g'`
        head="port"$pos/$port
    else
        tmp=`echo ${s1#*/}|sed 's/[^0-9,.:A-Za-z]//g'`
        port=`echo ${tmp%%.*}|sed 's/[^0-9,.:A-Za-z]//g'`
        tmp=`echo ${s1#*.}|sed 's/[^0-9,.:A-Za-z]//g'`
        line=`echo ${tmp%%:*}|sed 's/[^0-9,.:A-Za-z]//g'`
        head="port"$pos/$port"."$line
    fi
    identifier=`echo -n ${s1#*:}|sed 's/[^0-9,.:A-Za-z,-]//g'`
    
    input=$identifier$expend$privKey
    inputFile="."$identifier
    echo -n $input > $inputFile
    cat $inputFile
    echo 
#    echo $input
    output=`echo \`md5sum $inputFile\`|awk '{ print $1 }'`
    
#    echo $pos
#    echo $port
#    echo $identifier
#    echo $expend
#    echo $privKey
#    echo $input
#    echo $inputFile
#    echo $output
    echo $head: $output >> $licenseFile
    
    rm -rf $inputFile
done

echo "create license file : "$licenseFile
#cat $licenseFile
