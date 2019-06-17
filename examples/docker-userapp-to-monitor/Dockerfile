# NOTE: for this example we build our Docker on Ubuntu which provides a few utils like
# - lshw
# - stress
# out of the box!
FROM ubuntu:bionic

RUN apt-get update &&  apt-get -y install stress
COPY example-load.sh .
CMD /bin/bash example-load.sh ; bash
