for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -P "$file"
done
for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -P "$file" -c
done

for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -G "$file" -S "response"
    DIFF=$(diff "$file" "response")
    if [ "$DIFF" != "" ]
    then
        echo "$file did not transfer properly"
    fi
done

for file in ./*
do
    ../obj64/Client -s 192.168.88.235 -p 9000 -c -G "$file" -S "response"
    DIFF=$(diff "$file" "response")
    if [ "$DIFF" != "" ]
    then
        echo "$file did not transfer properly"
    fi
done
