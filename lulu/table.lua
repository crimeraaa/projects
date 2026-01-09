local z = 'z'
-- local t = {x=9, y=10, z=11}
-- print(t.x, t['y'], t[z])

-- t.x, t['y'], t[z] = t[z], t.y, t['x']
-- print(t.x, t['y'], t[z])

local t = {
    k = {x=9, ['y']=10, [z]=11},
}

print(t.k.x, t['k']['y'], t.k[z])
t['k'].x, t.k['y'], t['k'][z] = t.k.z, t.k.y, t.k.x
print(t.k.x, t.k.y, t.k.z)
