local rudp = Proto("_rudp", "Reliable UDP Protocol")

local fields = {
	magic = ProtoField.uint16("_rudp.magic", "Magic Number", base.HEX),
	version = ProtoField.uint8("_rudp.version", "Version", base.DEC),
	flags = ProtoField.uint8("_rudp.flags", "Flags", base.HEX),
	syn = ProtoField.bool("_rudp.flags.syn", "SYN", 8, nil, 0x01),
	ack = ProtoField.bool("_rudp.flags.ack", "ACK", 8, nil, 0x02),
	seqnum = ProtoField.uint32("_rudp.seqnum", "Sequence Number", base.DEC),
	acknum = ProtoField.uint32("_rudp.acknum", "Acknowledgment Number", base.DEC),
	length = ProtoField.uint32("_rudp.length", "Data Length", base.DEC),
	data = ProtoField.bytes("_rudp.data", "Data"),
}

rudp.fields = fields

function rudp.dissector(buffer, pinfo, tree)
	if buffer:len() < 16 then
		return 0
	end

	local magic = buffer(0, 2):uint()
	if magic ~= 0x1234 then
		return 0
	end

	pinfo.cols.protocol = "RUDP"

	local subtree = tree:add(rudp, buffer())

	subtree:add(fields.magic, buffer(0, 2))
	subtree:add(fields.version, buffer(2, 1))

	local flags = buffer(3, 1):uint()
	local flags_tree = subtree:add(fields.flags, buffer(3, 1))
	flags_tree:add(fields.syn, buffer(3, 1))
	flags_tree:add(fields.ack, buffer(3, 1))

	subtree:add(fields.seqnum, buffer(4, 4))
	subtree:add(fields.acknum, buffer(8, 4))

	local length = buffer(12, 4):uint()
	subtree:add(fields.length, buffer(12, 4))

	if length > 0 and buffer:len() >= 16 + length then
		subtree:add(fields.data, buffer(16, length))
	end

	local flag_strs = {}
	if bit.band(flags, 0x01) ~= 0 then
		table.insert(flag_strs, "SYN")
	end
	if bit.band(flags, 0x02) ~= 0 then
		table.insert(flag_strs, "ACK")
	end
	local flag_str = table.concat(flag_strs, ",")

	pinfo.cols.info = string.format(
		"v%d %u → %u SEQ=%u ACK=%u (LEN=%u)%s",
		buffer(2, 1):uint(),
		pinfo.src_port,
		pinfo.dst_port,
		buffer(4, 4):uint(),
		buffer(8, 4):uint(),
		length,
		flag_str ~= "" and " [" .. flag_str .. "]" or ""
	)

	return 16 + length
end

local function heuristic(buffer, pinfo, tree)
	if buffer:len() >= 16 and buffer(0, 2):uint() == 0x1234 then
		rudp.dissector(buffer, pinfo, tree)
		return true
	end
	return false
end

rudp:register_heuristic("udp", heuristic)
