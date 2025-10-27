def zip_with_fixed(list1, list2):
    """
    Эмулирует zip, но фиксирует один элемент из list2 и итерирует по list1,
    затем фиксирует следующий элемент из list2 и т.д.
    """
    for elem2 in list2:
        for elem1 in list1:
            yield (elem1, elem2)