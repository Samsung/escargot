for i in test/vendortest/driver/chakracore/*.rlexe.xml; do dir=`echo $i | cut -d'/' -f5 | cut -d'.' -f1`; \
echo "cp test/vendortest/driver/chakracore/$dir.rlexe.xml test/vendortest/ChakraCore/$dir/rlexe.xml"; \
cp test/vendortest/driver/chakracore/$dir.rlexe.xml test/vendortest/ChakraCore/$dir/rlexe.xml; done
echo "WScript.Echo('PASS');" >> test/vendortest/ChakraCore/DynamicCode/eval-nativecodedata.js
echo "WScript.Echo('PASS');" >> test/vendortest/ChakraCore/utf8/unicode_digit_as_identifier_should_work.js
test/vendortest/ChakraCore/run.sh ./escargot | tee test/vendortest/driver/chakracore.x86_64.gen.txt; \
diff test/vendortest/driver/chakracore.x86_64.orig.txt test/vendortest/driver/chakracore.x86_64.gen.txt
