
from g2p_en import G2p
import argparse
import sys

def english_g2p(text):
    g2p = G2p()
    unique_commands = []
    
    alphabet = {
        "AE1": "a",
        "N": "N",
        " ": " ",
        "OW1": "b",
        "V": "V",
        "AH0": "c",
        "L": "L",
        "F": "F",
        "EY1": "d",
        "S": "S",
        "B": "B",
        "R": "R",
        "AO1": "e",
        "D": "D",
        "AH1": "c",
        "EH1": "f",
        "OW0": "b",
        "IH0": "g",
        "G": "G",
        "HH": "h",
        "K": "K",
        "IH1": "g",
        "W": "W",
        "AY1": "i",
        "T": "T",
        "M": "M",
        "Z": "Z",
        "DH": "j",
        "ER0": "k",
        "P": "P",
        "NG": "l",
        "IY1": "m",
        "AA1": "n",
        "Y": "Y",
        "UW1": "o",
        "IY0": "m",
        "EH2": "f",
        "CH": "p",
        "AE0": "a",
        "JH": "q",
        "ZH": "r",
        "AA2": "n",
        "SH": "s",
        "AW1": "t",
        "OY1": "u",
        "AW2": "t",
        "IH2": "g",
        "AE2": "a",
        "EY2": "d",
        "ER1": "k",
        "TH": "v",
        "UH1": "w",
        "UW2": "o",
        "OW2": "b",
        "AY2": "i",
        "UW0": "o",
        "AH2": "c",
        "EH0": "f",
        "AW0": "t",
        "AO2": "e",
        "AO0": "e",
        "UH0": "w",
        "UH2": "w",
        "AA0": "n",
        "AY0": "i",
        "IY2": "m",
        "EY0": "d",
        "ER2": "k",
        "OY2": "u",
        "OY0": "u",
    }

    text_list = text.split(";")
    for phrase in text_list:
        labels = g2p(phrase)
        phoneme = ""
        for char in labels:
            if char in alphabet:
                phoneme += alphabet[char]
            # else ignore or handle unknowns?
            # Script user provided had check char not in alphabet -> print skip
        
        print(f'"{phrase}", "{phoneme}"')

if __name__ == "__main__":
    commands = "turn on light one;turn off light one;turn on light two;turn off light two;turn on socket;turn off socket;turn on fan at level one;turn on fan at level two;turn on fan at level three;turn off fan;check health;lock the door;unlock the door;walk forward aigis;stop aigis;lets dance aigis;tell a story"
    english_g2p(commands)
    
