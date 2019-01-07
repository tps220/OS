for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -P "$file" -e
done
for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -P "$file" -c -e
done

for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -G "$file" -S "response" -e
    DIFF=$(diff "$file" "response")
    if [ "$DIFF" != "" ]
    then
        echo "$file did not transfer properly"
    fi
done

for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -c -G "$file" -S "response" -e
    DIFF=$(diff "$file" "response")
    if [ "$DIFF" != "" ]
    then
        echo "$file did not transfer properly"
    fi
done
