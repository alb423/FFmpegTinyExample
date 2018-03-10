struct ring_buffer {
    void *address;
    unsigned long count_bytes;
    unsigned long write_offset_bytes;
    unsigned long read_offset_bytes;
};

extern void ring_buffer_create (struct ring_buffer *buffer, unsigned long order);
extern void ring_buffer_free(struct ring_buffer *buffer);
extern void *ring_buffer_write_address(struct ring_buffer *buffer);
extern void ring_buffer_write_advance(struct ring_buffer *buffer, unsigned long count_bytes);
extern void *ring_buffer_read_address(struct ring_buffer *buffer);
extern void ring_buffer_read_advance(struct ring_buffer *buffer, unsigned long count_bytes);
extern unsigned long ring_buffer_count_bytes(struct ring_buffer *buffer);
extern unsigned long ring_buffer_count_free_bytes(struct ring_buffer *buffer);
extern void ring_buffer_clear(struct ring_buffer *buffer);