function main(expected)
 local files=os.files("test/**.cc")
 table.sort(files)
 for _,file in ipairs(files) do print(file) end
 assert(#files==tonumber(expected),"unexpected test source discovery")
 print("PASS_ACTUAL_XMAKE_TEST_SOURCE_DISCOVERY count="..#files)
end
