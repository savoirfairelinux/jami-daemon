FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG config_args
RUN apt-get update && apt-get install python3-pip -y

RUN pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu
RUN pip3 install pandas \
    googletrans-py \
    transformers \
    transifex-python \
    openpyxl


RUN mkdir /.cache
RUN chmod 777 /.cache

ENV TRANSFORMERS_CACHE=/.cache