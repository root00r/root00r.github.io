#!/bash
for name in `ls *.html`
do
    echo $name
    newfile=`echo $name|sed 's/cd_feature_//g'`
    echo "newfile:$newfile"
    mv $name $newfile
done
