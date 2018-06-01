if cat test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null;
then exit 1;
fi;
