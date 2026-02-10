Command line used to find this crash:

/home/pirwani/SNPSFuzzer//afl-fuzz -d -i /home/pirwani/SNPSFuzzer/dnsmasq_workspace/fuzz_input -o /home/pirwani/SNPSFuzzer/dnsmasq_workspace/fuzz_output -N tcp://127.0.0.1/5353 -P DNS -D 10000 -K -R -a 1 -m none -- /home/pirwani/SNPSFuzzer/dnsmasq_workspace/dnsmasq/src/dnsmasq -C /home/pirwani/SNPSFuzzer/dnsmasq_workspace/dnsmasq.conf

If you can't reproduce a bug outside of afl-fuzz, be sure to set the same
memory limit. The limit used for this fuzzing session was 0 B.

Need a tool to minimize test cases before investigating the crashes or sending
them to a vendor? Check out the afl-tmin that comes with the fuzzer!

Found any cool bugs in open-source tools using afl-fuzz? If yes, please drop
me a mail at <lcamtuf@coredump.cx> once the issues are fixed - I'd love to
add your finds to the gallery at:

  http://lcamtuf.coredump.cx/afl/

Thanks :-)
