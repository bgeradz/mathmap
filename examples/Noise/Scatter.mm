filter scatter (image in, float distance: 0-100 (20))
    d=distance;
    in(xy+xy:[rand(-d,d),rand(-d,d)]*t)
end