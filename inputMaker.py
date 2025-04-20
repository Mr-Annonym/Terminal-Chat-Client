with open("input.txt", 'w') as f:
    f.write("/auth asdf asdf asdf\n")
    f.write("".join("a" for _ in range(60000)) + "\n")
    f.close()

    
