if ! cat ./test/octane_result | grep -c 'Score' > /dev/null; 
then exit 1;
fi;
