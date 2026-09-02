.section .data


.global gunbarrelBackground
gunbarrelBackground:
.ifdef VERSION_US
.incbin "build/u/assets/gunbarrel_background.bin"
.endif
.ifdef VERSION_DEBUG
.incbin "build/d/assets/gunbarrel_background.bin"
.endif
.global gunbarrelBackgroundEnd
gunbarrelBackgroundEnd:
