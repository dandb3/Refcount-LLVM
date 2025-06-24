struct gve_tx_ring {
	/* Cacheline 0 -- Accessed & dirtied during transmit */
	union {
		/* GQI fields */
		struct {
			struct gve_tx_fifo tx_fifo;
			u32 req; /* driver tracked head pointer */
			u32 done; /* driver tracked tail pointer */
		};

		/* DQO fields. */
		struct {
			/* Linked list of gve_tx_pending_packet_dqo. Index into
			 * pending_packets, or -1 if empty.
			 *
			 * This is a consumer list owned by the TX path. When it
			 * runs out, the producer list is stolen from the
			 * completion handling path
			 * (dqo_compl.free_pending_packets).
			 */
			s16 free_pending_packets;

			/* Cached value of `dqo_compl.hw_tx_head` */
			u32 head;
			u32 tail; /* Last posted buffer index + 1 */

			/* Index of the last descriptor with "report event" bit
			 * set.
			 */
			u32 last_re_idx;

			/* free running number of packet buf descriptors posted */
			u16 posted_packet_desc_cnt;
			/* free running number of packet buf descriptors completed */
			u16 completed_packet_desc_cnt;

			/* QPL fields */
			struct {
			       /* Linked list of gve_tx_buf_dqo. Index into
				* tx_qpl_buf_next, or -1 if empty.
				*
				* This is a consumer list owned by the TX path. When it
				* runs out, the producer list is stolen from the
				* completion handling path
				* (dqo_compl.free_tx_qpl_buf_head).
				*/
				s16 free_tx_qpl_buf_head;

			       /* Free running count of the number of QPL tx buffers
				* allocated
				*/
				u32 alloc_tx_qpl_buf_cnt;

				/* Cached value of `dqo_compl.free_tx_qpl_buf_cnt` */
				u32 free_tx_qpl_buf_cnt;
			};
		} dqo_tx;
	};

	/* Cacheline 1 -- Accessed & dirtied during gve_clean_tx_done */
	union {
		/* GQI fields */
		struct {
			/* Spinlock for when cleanup in progress */
			spinlock_t clean_lock;
			/* Spinlock for XDP tx traffic */
			spinlock_t xdp_lock;
		};

		/* DQO fields. */
		struct {
			u32 head; /* Last read on compl_desc */

			/* Tracks the current gen bit of compl_q */
			u8 cur_gen_bit;

			/* Linked list of gve_tx_pending_packet_dqo. Index into
			 * pending_packets, or -1 if empty.
			 *
			 * This is the producer list, owned by the completion
			 * handling path. When the consumer list
			 * (dqo_tx.free_pending_packets) is runs out, this list
			 * will be stolen.
			 */
			atomic_t free_pending_packets;

			/* Last TX ring index fetched by HW */
			atomic_t hw_tx_head;

			/* List to track pending packets which received a miss
			 * completion but not a corresponding reinjection.
			 */
			struct gve_index_list miss_completions;

			/* List to track pending packets that were completed
			 * before receiving a valid completion because they
			 * reached a specified timeout.
			 */
			struct gve_index_list timed_out_completions;

			/* QPL fields */
			struct {
				/* Linked list of gve_tx_buf_dqo. Index into
				 * tx_qpl_buf_next, or -1 if empty.
				 *
				 * This is the producer list, owned by the completion
				 * handling path. When the consumer list
				 * (dqo_tx.free_tx_qpl_buf_head) is runs out, this list
				 * will be stolen.
				 */
				atomic_t free_tx_qpl_buf_head;

				/* Free running count of the number of tx buffers
				 * freed
				 */
				atomic_t free_tx_qpl_buf_cnt;
			};
		} dqo_compl;
	} ____cacheline_aligned;
	u64 pkt_done; /* free-running - total packets completed */
	u64 bytes_done; /* free-running - total bytes completed */
	u64 dropped_pkt; /* free-running - total packets dropped */
	u64 dma_mapping_error; /* count of dma mapping errors */

	/* Cacheline 2 -- Read-mostly fields */
	union {
		/* GQI fields */
		struct {
			union gve_tx_desc *desc;

			/* Maps 1:1 to a desc */
			struct gve_tx_buffer_state *info;
		};

		/* DQO fields. */
		struct {
			union gve_tx_desc_dqo *tx_ring;
			struct gve_tx_compl_desc *compl_ring;

			struct gve_tx_pending_packet_dqo *pending_packets;
			s16 num_pending_packets;

			u32 complq_mask; /* complq size is complq_mask + 1 */

			/* QPL fields */
			struct {
				/* qpl assigned to this queue */
				struct gve_queue_page_list *qpl;

				/* Each QPL page is divided into TX bounce buffers
				 * of size GVE_TX_BUF_SIZE_DQO. tx_qpl_buf_next is
				 * an array to manage linked lists of TX buffers.
				 * An entry j at index i implies that j'th buffer
				 * is next on the list after i
				 */
				s16 *tx_qpl_buf_next;
				u32 num_tx_qpl_bufs;
			};
		} dqo;
	} ____cacheline_aligned;
	struct netdev_queue *netdev_txq;
	struct gve_queue_resources *q_resources; /* head and tail pointer idx */
	struct device *dev;
	u32 mask; /* masks req and done down to queue size */
	u8 raw_addressing; /* use raw_addressing? */

	/* Slow-path fields */
	u32 q_num ____cacheline_aligned; /* queue idx */
	u32 stop_queue; /* count of queue stops */
	u32 wake_queue; /* count of queue wakes */
	u32 queue_timeout; /* count of queue timeouts */
	u32 ntfy_id; /* notification block index */
	u32 last_kick_msec; /* Last time the queue was kicked */
	dma_addr_t bus; /* dma address of the descr ring */
	dma_addr_t q_resources_bus; /* dma address of the queue resources */
	dma_addr_t complq_bus_dqo; /* dma address of the dqo.compl_ring */
	struct u64_stats_sync statss; /* sync stats for 32bit archs */
	struct xsk_buff_pool *xsk_pool;
	u32 xdp_xsk_wakeup;
	u32 xdp_xsk_done;
	u64 xdp_xsk_sent;
	u64 xdp_xmit;
	u64 xdp_xmit_errors;
} ____cacheline_aligned;

int main() {
	union eng wow;
}