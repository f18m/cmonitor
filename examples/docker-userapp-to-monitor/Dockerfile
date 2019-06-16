FROM ubuntu:bionic

RUN apt-get update &&  apt-get -y install stress
COPY example-load.sh .
CMD /bin/bash example-load.sh ; bash
