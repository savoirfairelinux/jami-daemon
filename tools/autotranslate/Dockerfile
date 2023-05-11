FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG config_args
RUN apt-get update && apt-get install python3-pip -y

RUN pip3 install --index-url https://download.pytorch.org/whl/cpu \
    pandas \
    googletrans-py \
    transformers \
    torch \
    torchvision \
    torchaudio \
    transifex-python


RUN mkdir /.cache
RUN chmod 777 /.cache

ENV TRANSFORMERS_CACHE=/.cache