def raw_list_to_list(raw_list):
    new_list = list()

    for i, elem in enumerate(raw_list):
        new_list.append(elem.decode())

    return new_list

def raw_dict_to_dict(raw_dict):
    new_dict = dict()

    for key, value in raw_dict.items():
        new_dict[key.decode()] = value.decode()

    return new_dict
