lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. \nProin et dapibus libero, sed lacinia leo. Duis vel laoreet risus. Maecenas ac blandit sem, \net rutrum enim. Nunc vulputate fringilla risus sit amet feugiat. In in enim eleifend, condimentum nunc ac, \nporttitor dolor. Donec eu lectus justo. Nulla a finibus ipsum, non sagittis nisl. Quisque at fringilla nunc.\n Sed dapibus, justo dignissim rutrum mollis, enim elit fermentum urna, vel varius risus purus luctus dui. Phasellus eros est,\n euismod id urna mollis, semper dictum quam. Phasellus neque enim, fringilla cursus nisl at, lacinia imperdiet ipsum.\n Phasellus vel fermentum leo. Pellentesque sed libero tempor, porttitor lacus id, maximus nulla.\n"
np = int(input("n = 100 x "))
with open("input.txt", "w") as f:
    for i in range(np):
        f.write(lorem)
