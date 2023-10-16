FROM circleci/buildpack-deps:jessie

USER root

RUN rm /etc/apt/sources.list && \
    echo 'deb http://deb.freexian.com/extended-lts jessie main contrib non-free' > /etc/apt/sources.list.d/extended-lts.list && \
    wget --no-check-certificate https://deb.freexian.com/extended-lts/archive-key.gpg -O /tmp/elts-archive-key.gpg &&\
    mv /tmp/elts-archive-key.gpg /etc/apt/trusted.gpg.d/freexian-archive-extended-lts.gpg && \
    apt-get -y update
RUN apt-get -y install make autopoint texinfo libtool intltool bzip2 gettext clang-3.5 clang-4.0 g++-multilib && \
    apt-get -y purge aspell

USER circleci
